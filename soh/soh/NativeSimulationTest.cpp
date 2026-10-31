#include "StalfosTemporal.h"
// Opt-in observational 20-Hz replay. No gameplay arithmetic lives in this module.
#include "NativeSimulationTest.hpp"
#include "NativeSimulationValidation.hpp"
#include "PlayerTemporal.h"
#include "frame_interpolation.h"
#include "first_person_view.h"
#include "PlayerContactBridge.h"
#include "PlayerTemporalCore.hpp"
#include "PlayerSchedulerCore.hpp"
#include <chrono>
#include <array>
#include <thread>
#include <libultraship/bridge/windowbridge.h>
#include <fast/ShaderCompilationStats.h>
#include "NativeSimulationPresentation.h"
#include "NativeSimulationPresentationCoverage.hpp"
#include "NativeSimulationHudObservation.h"
#include "NativeSimulationMessageObservation.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/gameconsole.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
extern "C" {
#include "global.h"
#include "player_pose.h"
#include "player_animation.h"
#include "player_animation_sample.h"
#include "player_input.h"
extern EffectContext sEffectContext;
void OTRAudio_Exit(void);
extern EffectSsInfo sEffectSsInfo;
#include "regs.h"
#include "message_data_textbox_types.h"
#include "overlays/actors/ovl_En_Kanban/z_en_kanban.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_Horse/z_en_horse.h"
#include "overlays/actors/ovl_En_Test/z_en_test.h"
void EnTest_SetupSlashDown(EnTest*);
void EnTest_SlashDown(EnTest*, PlayState*);
void func_80862FA8(EnTest*, PlayState*);
#include "overlays/actors/ovl_En_Door/z_en_door.h"
#include "overlays/actors/ovl_Arms_Hook/z_arms_hook.h"
void ArmsHook_Wait(ArmsHook* hook, PlayState* play);
void ArmsHook_PullPlayer(ArmsHook* hook);
s32 Player_StartFishing(PlayState* play);
void Player_ZeroSpeedXZ(Player* player);
s32 func_8084B3CC(PlayState* play, Player* player);
void func_8083A0F4(PlayState* play, Player* player);
s32 Player_SetStartingMovement(PlayState* play, Player* player, f32 distance);
s32 Player_HandleSlopes(PlayState* play, Player* player, CollisionPoly* floor);
void func_80837B9C(Player* player, PlayState* play);
void func_8083C0E8(Player* player, PlayState* play);
void Player_SetupRoll(Player* player, PlayState* play);
void func_80864158(EnTest* enemy, f32 speed);
s32 func_80838FB8(PlayState* play, Player* player);
}

using nlohmann::json;
namespace {
bool enabled = false, measuring = false, verbose = false;
EnTest* stalfosTestActor=nullptr;
json stalfosFrames=json::array();
json rollFrames=json::array();
uint64_t stalfosFirstExit=UINT64_MAX, stalfosWorldUpdates=0;
int stalfosInitialHealth=0, stalfosPlayerHealth=0;
bool performanceOnly = false;
struct FrameTiming {
    double previous = 0, windowStart = 0;
    uint64_t intervals = 0, slowFrames = 0, windows = 0, maximumFrame = 0, windowIntervals = 0, excluded = 0;
    double totalMs = 0, maximumMs = 0, worstWindowMs = 0;
    std::array<uint64_t, 2001> histogram{}; // 0.05-ms buckets, bounded at100ms
    NativeValidation::Ring<double, 64> tail;
    std::array<uint64_t, 6> slowByOffset{};
    void Add(double now, bool exclude = false) {
        if (exclude) {
            ++excluded;
            previous = windowStart = now;
            windowIntervals = 0;
            return;
        }
        if (previous) {
            double ms = (now - previous) * 1000;
            ++intervals; totalMs += ms;
            if (ms > maximumMs) { maximumMs = ms; maximumFrame = intervals; }
            slowFrames += ms > 12.5;
            if (ms > 12.5) ++slowByOffset[intervals % 6];
            ++histogram[std::min<size_t>(2000, static_cast<size_t>(ms * 20))];
            tail.Push(ms);
            if (++windowIntervals == 120) {
                worstWindowMs = std::max(worstWindowMs,
                    (now - windowStart) * 1000);
                windowStart = now; ++windows; windowIntervals = 0;
            }
        } else windowStart = now;
        previous = now;
    }
    double Percentile(double fraction) const {
        uint64_t count = 0;
        for (size_t i = 0; i < histogram.size(); ++i) {
            count += histogram[i];
            if (count >= static_cast<uint64_t>(std::ceil(intervals * fraction))) return (i + 1) * 0.05;
        }
        return maximumMs;
    }
} frameTiming, completionTiming, steadyFrameTiming;
Fast::ShaderCompilationStats previousShaderStats;
uint64_t measuredShaderCompilations = 0, measuredShaderCacheHits = 0;
double measuredShaderCompileMs = 0;
bool previousFrameCompiled = false;
double lastInputPoll = 0, maxInputToPresentMs = 0, totalInputToPresentMs = 0;
uint64_t inputToPresentSamples = 0, inputToPresentOverBudget = 0;
bool verifyPresentationPurity = false, purityNegativeControl = false;
bool observeTemporal = false, singleStepControl = false;
bool detailedSnapshots = false;
std::filesystem::path referenceCheckpoints;
json checkpoints = json::array(), expectedCheckpoints;
std::unique_ptr<NativeValidation::Assertions> onlineAssertions;
NativeValidation::Ring<json, 4> boundaryRing;
void DumpFailureDiagnostics();
PlayerTemporal::CanonicalControl qaControl;
PlayerTemporal::PlayerStepControl qaPlayerControl;
uint64_t qaSequence = 0, qaHolds = 0;
json qaPlayerCommands = json::array();
std::map<std::string, unsigned> worldOpportunities;
std::ofstream temporalSnapshots;
std::ofstream playerStepSnapshots;
std::ofstream playerInputEvents;
json purityCoverage = json::object(), admissionCoverage = json::object();
json fixture, previousPhase;
std::filesystem::path output;
std::ofstream snapshots, trace;
std::vector<std::string> pendingTrace;
uint64_t tick = 0, setupFrames = 0, engineFrames = 0, audioBlocks = 0, audioSamples = 0, sequence = 0;
uint64_t spawnOrdinal = 0, sceneEpoch = 0;
uint32_t audioClockCalls = 0;
int updateCalls = 0, drawCalls = 0;
uint64_t playerPoseGeneration = 0, playerContacts = 0;
std::string phase = "initialization";
std::unordered_map<Actor*, uint64_t> actorIds;
Actor* currentActor = nullptr;
Actor* distantTarget = nullptr;
Actor* bridgeTarget = nullptr;
uint64_t bridgeTargetUpdates = 0, bridgeCuts = 0;
uint16_t bridgeParts = 0xffff;
int16_t bridgeCooldown = 0;
bool bridgeInvalidated = false;
Actor* interactionTarget = nullptr;
std::string interactionKind;
uint64_t interactionUpdates = 0, interactionTalkQ = UINT64_MAX, interactionWorldEdges = 0, interactionOverlaps = 0;
uint64_t interactionTextboxQ = UINT64_MAX;
uint64_t dialogueClosingChecks = 0;
bool dialogueCloseInjected = false, dialogueClosingStep = false;
uint64_t interactionBushes = 0, interactionContacts = 0;
bool interactionDestroyed = false;
json mechanicSamples = json::array();
json spinEvents = json::array();
const void* spinAnimation = nullptr;
int spinStage = -2, spinActionVar = -1000;
bool spinReleasing = false;
json Animation(SkelAnime& animation);

void ObserveSpin(Player* p, uint64_t timeQ) {
    if (fixture.value("mechanic_case", std::string()) != "spin") return;
    const bool charging = (p->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) != 0;
    // EnMThunder consumes/clears the request flag later in the world tick.
    // Observe the actual melee action as well, at either sampling boundary.
    const bool releasing = (p->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) != 0 ||
        (p->meleeWeaponAnimation >= PLAYER_MWA_SPIN_ATTACK_1H &&
         std::strcmp(NativeSimTest_PlayerActionName(p), "Player_Action_808502D0") == 0);
    const int stage = !charging ? -1 : p->unk_858 < .1f ? 0 : p->unk_858 <= .15f ? 1 : p->unk_858 < .85f ? 2 : 3;
    if (spinAnimation != p->skelAnime.animation || stage != spinStage || releasing != spinReleasing ||
        (charging && spinActionVar != p->av2.actionVar2)) {
        spinEvents.push_back({{"q",timeQ},{"charge",p->unk_858},{"stage",stage},{"spin",releasing},
            {"action_var",p->av2.actionVar2},{"frame",p->skelAnime.curFrame},
            {"action",NativeSimTest_PlayerActionName(p)},{"melee_animation",p->meleeWeaponAnimation},
            {"magic",gSaveContext.magic},{"magic_state",gSaveContext.magicState},
            {"animation",Animation(p->skelAnime)}});
        spinAnimation = p->skelAnime.animation; spinStage = stage;
        spinReleasing = releasing; spinActionVar = p->av2.actionVar2;
    }
}
int mechanicHealth = 0;
uint64_t mechanicHeldBombSteps = 0, mechanicHeldExplosionSteps = 0, mechanicDamageSteps = 0;
uint64_t mechanicCrawlSteps = 0, mechanicCrawlCameraSteps = 0;
uint64_t mechanicMountedTicks = 0, mechanicHorseAEdges = 0, mechanicCarrotsUsed = 0;
int mechanicLastCarrots = -1;
uint64_t mechanicWaterTicks = 0, mechanicCurrentTicks = 0, aimResponseChecks = 0;
float mechanicBodyMaxStep = 0;
uint64_t mechanicBodySteps = 0;
json mechanicBodyPeak;
uint64_t highWorldTransactions = 0, previousWorldHighSteps = 0;
uint64_t phaseContactOrder = 14695981039346656037ull, phaseContactEvents = 0;
json phaseContactBoundaries = json::array();
uint32_t liveState2Seen = 0;
std::map<uint32_t, uint64_t> liveState2Samples;
uint64_t liveHintSamples = 0, liveStatusFrames = 0;
uint64_t renderAttempts = 0, renderedFrames = 0, firstPersonFrames = 0, worldCUpEdges = 0;
uint64_t currentPlayerPoseSubmissions = 0;
json nextFrameResponses = json::array();
PlayerPosePacket previousPresentedPose{};
bool havePresentedPose = false;
Player previousPresentedPlayer{};
Player previousTransitionPlayer{};
bool haveTransitionPlayer = false;
std::ofstream actionTransitions;
uint64_t transitionActionStart = 0;
std::chrono::steady_clock::time_point renderStart;
void ObserveLiveIndicators(Player* player) {
    if (!fixture.value("observe_live_indicators",false)) return;
    liveState2Seen |= player->stateFlags2;
    ++liveState2Samples[player->stateFlags2];
    liveHintSamples += player->naviTextId > 0;
}
std::vector<Actor*> actorScope;
struct Stream { uint64_t calls = 0, drawCalls = 0; uint32_t state = 0; uint64_t order = 14695981039346656037ull; };
std::map<std::string, Stream> streams;
[[noreturn]] void Fail(const std::string& message);
struct StepObservation {
    PlayerTemporalObservation temporal{};
    Player player{}; // private bounded copies, pointers are never followed by the dumper
    uint64_t worldTick=0;
    Vec3f targetPosition{},targetFocus{};
    Vec3s cameraInput{};
    int cameraMode=0;
};
NativeValidation::Ring<StepObservation,32> stepRing;
StepObservation previousStep;
bool havePreviousStep=false, posePending=false;
Player poseBefore{};
uint64_t highSteps=0,motionChecks=0,movingChecks=0,animationChecks=0,cameraChanges=0,targetChecks=0;
uint64_t poseChecks=0,warmChecks=0,sweepChecks=0,noMotionChecks=0,targetWorldUpdates=0;
json controlChecks=json::array();
bool Equal(Vec3f a,Vec3f b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
void DumpFailureDiagnostics() {
    // Do not call gameplay/Float() here: this path must also handle nonfinite state.
    try {
        std::ofstream boundaries(output/"failure-boundaries.jsonl");
        boundaryRing.Visit([&](const json& row){boundaries<<row.dump()<<'\n';});
        std::ofstream steps(output/"failure-player-steps.jsonl");
        stepRing.Visit([&](const StepObservation& s){
            auto vec=[](Vec3f v){return json::array({v.x,v.y,v.z});};
            json weapons=json::array(),joints=json::array(),quads=json::array();
            for (const auto& w:s.player.meleeWeaponInfo) weapons.push_back({{"active",w.active},{"base",vec(w.base)},{"tip",vec(w.tip)}});
            for (const auto& j:s.player.jointTable) joints.push_back({j.x,j.y,j.z});
            for (const auto& q:s.player.meleeWeaponQuads) { json points=json::array();for (auto v:q.dim.quad) points.push_back(vec(v));quads.push_back(points); }
            steps<<json{{"tick",s.worldTick},{"step",s.temporal.step},{"start_q",s.temporal.start},{"end_q",s.temporal.end},
                {"pose",s.temporal.pose},{"animation_generation",s.temporal.animation},
                {"input_sequence",s.temporal.sequence},{"pressed",s.temporal.pressed},{"released",s.temporal.released},
                {"position",vec(s.player.actor.world.pos)},{"velocity",vec(s.player.actor.velocity)},
                {"frame",s.player.skelAnime.curFrame},{"speed",s.player.skelAnime.playSpeed},
                {"yaw",s.player.yaw},{"melee_state",s.player.meleeWeaponState},{"bg_flags",s.player.actor.bgCheckFlags},
                {"joints",joints},{"weapons",weapons},{"quads",quads},
                {"target_position",vec(s.targetPosition)},{"target_focus",vec(s.targetFocus)},
                {"camera_mode",s.cameraMode},{"camera_input",{s.cameraInput.x,s.cameraInput.y,s.cameraInput.z}}}.dump()<<'\n';
        });
    } catch (...) { /* Preserve the original diagnostic failure even if disk is full. */ }
}
void CheckPose(const char* site, Player* player) {
    if (!measuring || !PlayerTemporal_Observe().high) return;
    auto fail=[&](const char* message) {
        StepObservation current;current.player=*player;current.temporal=PlayerTemporal_Observe();current.worldTick=tick;
        stepRing.Push(current);Fail(message);
    };
    const bool begin=std::strcmp(site,"pose.begin")==0 || std::strcmp(site,"player_step.begin")==0;
    const bool end=std::strcmp(site,"pose.end")==0 || std::strcmp(site,"player_step.end")==0;
    if (begin) { if (posePending) fail("overlapping authoritative pose");poseBefore=*player;posePending=true; }
    if (!end) return;
    if (!posePending) fail("missing authoritative pose begin");
    ++poseChecks;posePending=false;
    for (unsigned i=1;i<3;++i) {
        const auto& a=poseBefore.meleeWeaponInfo[i];const auto& b=player->meleeWeaponInfo[i];
        const auto& old=poseBefore.meleeWeaponQuads[i-1].dim.quad;const auto& now=player->meleeWeaponQuads[i-1].dim.quad;
        bool same=true;for (unsigned j=0;j<4;++j) same &= Equal(old[j],now[j]);
        if (!a.active && b.active) { if (!same) fail("sword warm-up changed swept quad");++warmChecks; }
        else if (a.active && b.active && poseBefore.meleeWeaponState>0 && player->meleeWeaponState>0) {
            if (Equal(a.base,b.base) && Equal(a.tip,b.tip)) {
                if (!same) fail("unchanged sword endpoints changed swept quad");++noMotionChecks;
            } else {
                if (!Equal(now[0],b.base) || !Equal(now[1],b.tip) || !Equal(now[2],a.base) || !Equal(now[3],a.tip))
                    fail("sword sweep lost committed endpoint history");
                ++sweepChecks;
            }
        }
    }
}

std::string Hex(uint64_t value, int width) {
    std::ostringstream str;
    str << std::hex << std::setfill('0') << std::setw(width) << value;
    return str.str();
}
[[noreturn]] void Fail(const std::string& message) {
    if (!output.empty()) DumpFailureDiagnostics();
    if (!output.empty() && !std::filesystem::exists(output / "result.json")) {
        std::ofstream file(output / "result.json");
        file << json{{"schema", 1}, {"status", "fail"}, {"error", message}, {"tick", tick},
                     {"phase", phase}, {"ticks_completed", tick}}.dump(2) << '\n';
    }
    if (snapshots.is_open()) snapshots.flush();
    if (trace.is_open()) trace.flush();
    fprintf(stderr, "Native simulation test failed at tick %llu (%s): %s\n",
            static_cast<unsigned long long>(tick), phase.c_str(), message.c_str());
    if (performanceOnly) OTRAudio_Exit();
    std::exit(2);
}

// These fixtures seed status state once, then observe the real Player update.
// They do not run a duplicate status helper or replace the Player action.
struct PlayerStatusState {
    int shock = 0, shockPhase = 0, health = 0;
    bool burning = false;
    uint64_t effectEpochs = 0;
    std::array<uint8_t, PLAYER_BODYPART_MAX> flames{};
};
struct PlayerStatusChecks {
    bool open = false, world = false, shockSeeded = false, burnSeeded = false;
    bool extinguishSeeded = false, extinguishInjected = false;
    uint64_t worlds = 0, intermediates = 0, shockTicks = 0, burnTicks = 0;
    uint64_t shockExpiries = 0, burnExpiries = 0, damagePulses = 0, burnStartTick = 0;
    uint64_t extinguishTicks = 0, extinguishExpiries = 0, extinguishDamagePulses = 0;
    uint64_t extinguishEffectChecks = 0, intermediateEffectChecks = 0;
    int initialHealth = 0;
    PlayerStatusState before;
} playerStatusChecks;

PlayerStatusState ReadPlayerStatus(Player* player) {
    PlayerStatusState state;
    state.shock = player->bodyShockTimer;
    state.shockPhase = player->unk_892;
    state.health = gSaveContext.health;
    state.burning = player->bodyIsBurning != 0;
    // Slot epochs increase on effect insertion, including replacement of a
    // still-live effect. Unlike live counts, they detect duplicate emissions.
    for (int i = 0; i < sEffectSsInfo.tableSize; ++i) state.effectEpochs += sEffectSsInfo.table[i].epoch;
    std::memcpy(state.flames.data(), player->bodyFlameTimers, state.flames.size());
    return state;
}

void ObservePlayerStatus(const char* site, PlayState* play) {
    if (!enabled || !measuring || !fixture.value("test_player_status_cadence", false)) return;
    const bool worldBegin = !std::strcmp(site, "actor.update.begin");
    const bool intermediateBegin = !std::strcmp(site, "player_step.begin");
    const bool worldEnd = !std::strcmp(site, "actor.update.end");
    const bool intermediateEnd = !std::strcmp(site, "player_step.end");
    if (!worldBegin && !intermediateBegin && !worldEnd && !intermediateEnd) return;
    auto* player = GET_PLAYER(play);
    auto& check = playerStatusChecks;
    auto require = [&](bool condition, const std::string& detail) {
        if (!condition) Fail(std::string("Player status cadence at ") + site + ": " + detail);
    };
    if (worldBegin || intermediateBegin) {
        require(!check.open, "missing previous end observation");
        if (worldBegin && !check.shockSeeded) {
            require(tick == 0 && !player->bodyShockTimer && !player->bodyIsBurning,
                    "status fixture did not start cleanly");
            require(gSaveContext.health > 20 && player->invincibilityTimer == 0,
                    "status fixture requires healthy, vulnerable Player");
            player->bodyShockTimer = 40;
            player->unk_892 = 0;
            check.initialHealth = gSaveContext.health;
            check.shockSeeded = true;
        }
        if (worldBegin && !check.burnSeeded && tick >= 42 && (play->gameplayFrames & 7) == 0) {
            require(player->bodyShockTimer == 0, "shock did not expire before burn stage");
            // Twenty world decrements cover damage opportunities at0,8,16,
            // followed by a zero/expired stage. No shield destruction is seeded.
            player->bodyIsBurning = true;
            std::memset(player->bodyFlameTimers, 20, sizeof(player->bodyFlameTimers));
            check.burnStartTick = tick;
            check.burnSeeded = true;
        }
        if (worldBegin && tick == 72) {
            require(!player->bodyIsBurning && check.burnTicks == 20 && check.burnExpiries == 1 &&
                        check.damagePulses == 3 && gSaveContext.health == check.initialHealth - 3,
                    "natural burn stage incomplete before extinguish stage");
            player->bodyIsBurning = true;
            std::memset(player->bodyFlameTimers, 20, sizeof(player->bodyFlameTimers));
            check.extinguishSeeded = true;
        }
        const bool injectFootstep = check.extinguishSeeded && !check.extinguishInjected &&
            ((intermediateBegin && tick == 72) ||
             (worldBegin && tick == 73 && fixture.value("player_hz", 20) == 20));
        if (injectFootstep) {
            // Inject the incoming event only. The production Player update
            // must clear its flag and retain its world-owned extinguish effect.
            // This covers event retention, not a running/rolling animation.
            player->stateFlags2 |= PLAYER_STATE2_FOOTSTEP;
            check.extinguishInjected = true;
        }
        require(player->linearVelocity == 0.0f &&
                    (!(player->stateFlags2 & PLAYER_STATE2_FOOTSTEP) || injectFootstep) &&
                    !(player->stateFlags1 & (PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DEAD)) &&
                    player->invincibilityTimer == 0 && player->currentShield != PLAYER_SHIELD_DEKU &&
                    player->currentTunic != PLAYER_TUNIC_GORON,
                "isolated idle/status preconditions changed");
        check.before = ReadPlayerStatus(player);
        check.world = worldBegin;
        check.open = true;
        return;
    }

    require(check.open && check.world == worldEnd, "unpaired status observation");
    const auto after = ReadPlayerStatus(player);
    const auto& before = check.before;
    const int expectedShock = check.world && before.shock ? before.shock - 1 : before.shock;
    require(after.shock == expectedShock, "shock expected " + std::to_string(expectedShock) +
                ", got " + std::to_string(after.shock));
    if (!check.world) {
        require(after.shockPhase == before.shockPhase, "intermediate shock effect advanced");
        require(after.effectEpochs == before.effectEpochs, "intermediate status effect emitted");
        ++check.intermediateEffectChecks;
    }
    int expectedFlame = before.flames[0];
    if (check.world && before.burning && expectedFlame > 0) --expectedFlame;
    if (check.world && tick == 73) {
        require(check.extinguishInjected, "extinguish event was not injected");
        expectedFlame = 0;
        require(after.effectEpochs == before.effectEpochs, "extinguished burn emitted another effect");
        ++check.extinguishEffectChecks;
    }
    require(!(player->stateFlags2 & PLAYER_STATE2_FOOTSTEP), "incoming footstep event was not cleared");
    for (size_t i = 0; i < after.flames.size(); ++i) {
        const int expected = check.world && before.burning ? expectedFlame : before.flames[i];
        require(after.flames[i] == expected, "flame " + std::to_string(i) + " expected " +
                    std::to_string(expected) + ", got " + std::to_string(after.flames[i]));
    }
    const bool expectedBurning = check.world && before.burning ? expectedFlame > 0 : before.burning;
    require(after.burning == expectedBurning, "burn expiry changed at wrong opportunity");
    const bool damageDue = check.world && before.burning && expectedFlame > 0 &&
        (play->gameplayFrames & 7) == 0;
    const int expectedHealth = before.health - (damageDue ? 1 : 0);
    require(after.health == expectedHealth, "health expected " + std::to_string(expectedHealth) +
                ", got " + std::to_string(after.health));
    if (check.world) {
        ++check.worlds;
        check.shockTicks += before.shock > 0;
        check.shockExpiries += before.shock > 0 && after.shock == 0;
        if (check.extinguishSeeded) {
            check.extinguishTicks += before.burning;
            check.extinguishExpiries += before.burning && !after.burning;
            check.extinguishDamagePulses += damageDue;
        } else {
            check.burnTicks += before.burning;
            check.burnExpiries += before.burning && !after.burning;
            check.damagePulses += damageDue;
        }
    } else ++check.intermediates;
    check.open = false;
}

void FinishPlayerStatusChecks() {
    if (!fixture.value("test_player_status_cadence", false)) return;
    const auto& check = playerStatusChecks;
    const unsigned playerHz = fixture.value("player_hz", 20);
    const bool passed = !check.open && check.shockSeeded && check.burnSeeded && check.worlds == tick &&
        check.intermediates == tick * (playerHz / 20 - 1) && check.shockTicks == 40 && check.burnTicks == 20 &&
        check.shockExpiries == 1 && check.burnExpiries == 1 && check.damagePulses == 3 &&
        check.extinguishSeeded && check.extinguishInjected && check.extinguishTicks == 2 &&
        check.extinguishExpiries == 1 && check.extinguishEffectChecks == 1 &&
        check.intermediateEffectChecks == check.intermediates && check.extinguishDamagePulses <= 1 &&
        gSaveContext.health == check.initialHealth - 3 - static_cast<int>(check.extinguishDamagePulses);
    const json report = {{"status", passed ? "pass" : "fail"}, {"player_hz", playerHz},
        {"world_checks", check.worlds}, {"intermediate_checks", check.intermediates},
        {"shock_ticks", check.shockTicks}, {"shock_expiries", check.shockExpiries},
        {"burn_start_tick", check.burnStartTick}, {"burn_ticks", check.burnTicks},
        {"burn_expiries", check.burnExpiries}, {"damage_pulses", check.damagePulses},
        {"extinguish_event_injected", check.extinguishInjected}, {"extinguish_ticks", check.extinguishTicks},
        {"extinguish_expiries", check.extinguishExpiries}, {"extinguish_damage_pulses", check.extinguishDamagePulses},
        {"extinguish_no_effect_checks", check.extinguishEffectChecks},
        {"intermediate_no_effect_checks", check.intermediateEffectChecks},
        {"health_delta", gSaveContext.health - check.initialHealth}};
    { std::ofstream file(output / "player-status-cadence.json"); file << report.dump(2) << '\n';
      if (!file) Fail("Player status receipt write failed"); }
    if (!passed) Fail("Player status cadence coverage incomplete: " + report.dump());
}

// Bounded action tests use the real damage entry and a real hook actor's latch
// handoff. They do not claim to exercise an enemy collision or a hookable target.
struct ActionRateChecks {
    std::string kind;
    bool open = false, world = false, beforeActive = false, entered = false, finished = false;
    bool beforeParent = false, analogStruggle = false;
    int beforeTimer = 0, beforePhase = 0, beforeHealth = 0, initialHealth = 0;
    uint16_t pressed = 0;
    Vec3f position{}, velocity{};
    ArmsHook* hook = nullptr;
    uint64_t samples = 0, worldCounts = 0, intermediateChecks = 0, damagePulses = 0;
    uint64_t inputEdges = 0, firstInputQ = 0, entryQ = 0, releaseQ = 0, exitQ = 0, motionChecks = 0;
    uint64_t analogProgress = 0;
    double maxMotion = 0, travel = 0;
} actionRateChecks;

void ObserveActionRate(const char* site, PlayState* play) {
    auto& c = actionRateChecks;
    if (!enabled || !measuring || c.kind.empty()) return;
    const bool begin = !std::strcmp(site, "actor.update.begin") || !std::strcmp(site, "player_step.begin");
    const bool end = !std::strcmp(site, "actor.update.end") || !std::strcmp(site, "player_step.end");
    const bool hookMotion = !std::strcmp(site, "hookshot.motion.end");
    if (!begin && !end && !hookMotion) return;
    auto* p = GET_PLAYER(play);
    const auto time = PlayerTemporal_Observe();
    const uint64_t q = tick * 6 + (time.high ? time.start % 6 : 0);
    const bool hook = c.kind == "hookshot", freeze = c.kind == "freeze";
    const char* name = hook ? "Player_Action_80850AEC" : freeze ? "Player_Action_8084FB10" : "Player_Action_8084FBF4";
    auto active = [&]() { return !std::strcmp(NativeSimTest_PlayerActionName(p), name); };
    auto require = [&](bool okay, const char* why) {
        if (!okay) Fail(c.kind + " action rate at q" + std::to_string(q) + ": " + why);
    };
    if (hookMotion) {
        if (!hook) return;
        require(c.open && c.beforeActive, "hook motion outside its action");
        // Observe the pull before queued animation translation. Intermediate
        // step.end includes root motion; canonical actor.update.end precedes it.
        const float gain = time.high ? float(time.end - time.start) / 6.0f : 1.0f;
        const float before[] = {c.position.x,c.position.y,c.position.z};
        const float velocity[] = {c.velocity.x,c.velocity.y,c.velocity.z};
        const float after[] = {p->actor.world.pos.x,p->actor.world.pos.y,p->actor.world.pos.z};
        double squared = 0;
        for (int i = 0; i < 3; ++i) {
            require(std::isfinite(after[i]) && std::fabs(after[i] - (before[i] + velocity[i] * gain)) < 0.002f,
                    "hook displacement did not match world-authored velocity duration");
            squared += double(after[i]-before[i]) * (after[i]-before[i]);
        }
        const double distance = std::sqrt(squared);
        if (c.beforeParent) {
            require(distance <= 30.001 * gain, "hook exceeded its 30-units-per-world pull cap");
            c.maxMotion = std::max(c.maxMotion,distance); c.travel += distance; ++c.motionChecks;
        }
        return;
    }
    if (begin) {
        require(!c.open, "unpaired begin");
        c.world = !std::strcmp(site, "actor.update.begin");
        // Observation retains the last consumed event. At begin, time.step is
        // the previous committed step, so only its successor owns a fresh edge.
        c.pressed = c.world ? play->state.input[0].press.button :
            (time.consumingStep == time.step + 1 ? time.pressed : 0);
        if (!hook && c.world && tick == 2) {
            require((p->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !p->invincibilityTimer,
                    "damage entry requires grounded vulnerable Player");
            c.initialHealth = gSaveContext.health;
            p->actor.colChkInfo.damage = 4;
            func_80837C0C(play, p, freeze ? PLAYER_HIT_RESPONSE_FROZEN : PLAYER_HIT_RESPONSE_ELECTRIFIED,
                         0.0f, 0.0f, p->yaw, 20);
            require(active() && p->av2.actionVar2 == (freeze ? 0 : 20), "real damage entry failed");
            require(gSaveContext.health == c.initialHealth - 4, "initial damage was not applied once");
        }
        if (hook && c.world && tick == 30) {
            require(Player_HoldsHookshot(p) && p->heldActor && p->heldActor->id == ACTOR_ARMS_HOOK,
                    "C-item input did not produce a held hook");
            c.hook = reinterpret_cast<ArmsHook*>(p->heldActor);
            // Author the latch boundary, not a Player action or a flight velocity.
            // Subsequent world ArmsHook updates own pull velocity and release.
            p->actor.child = p->heldActor = nullptr;
            c.hook->actor.parent = nullptr;
            ArmsHook_Wait(c.hook, play);
            c.hook->timer = 0;
            c.hook->actor.world.pos = {p->unk_3C8.x + 300.0f, p->unk_3C8.y + 120.0f, p->unk_3C8.z};
            ArmsHook_PullPlayer(c.hook);
        }
        c.beforeActive = active(); c.beforeTimer = p->av2.actionVar2; c.beforePhase = p->av1.actionVar1;
        c.beforeHealth = gSaveContext.health; c.position = p->actor.world.pos; c.velocity = p->actor.velocity;
        c.beforeParent = c.hook && p->actor.parent == &c.hook->actor;
        c.open = true;
        return;
    }
    require(c.open, "unpaired end"); ++c.samples;
    if (!c.entered && active()) { c.entered = true; c.entryQ = q; }
    if (c.beforeActive && !c.finished) {
        if (!hook && (!freeze || c.beforePhase >= 0)) {
            const int edge = freeze && (c.pressed & (BTN_A | BTN_B)) ? 5 : 0;
            const int after = active() ? p->av2.actionVar2 : 0;
            const int expected = c.beforeTimer + (freeze ? int(c.world)+edge : -int(c.world));
            if (c.analogStruggle) {
                const int analog = after - expected;
                require(analog >= 0 && analog <= 5 && (c.world || analog == 0),
                        "analog struggle escaped world-sample ownership");
                c.analogProgress += analog;
            } else {
                require(after == expected, "action timer or struggle input has incorrect cadence");
            }
            if (c.world) ++c.worldCounts; else ++c.intermediateChecks;
            if (edge) { if (!c.inputEdges) c.firstInputQ = q; ++c.inputEdges; }
            const int damage = freeze && c.world && play->gameplayFrames % 4 == 0 &&
                !p->invincibilityTimer && !Player_InBlockingCsMode(play,p) ? 1 : 0;
            require(gSaveContext.health == c.beforeHealth - damage, "periodic damage repeated or skipped");
            c.damagePulses += damage;
            if (freeze && p->av1.actionVar1 < 0 && !c.releaseQ) c.releaseQ = q;
        }
        if (!active()) {
            c.finished = true; c.exitQ = q;
            if (hook) require(!p->actor.parent && p->heldActor == &c.hook->actor, "hook release ownership failed");
        }
    }
    c.open = false;
}

void FinishActionRate() {
    const auto& c = actionRateChecks;
    if (c.kind.empty()) return;
    const bool hook = c.kind == "hookshot", freeze = c.kind == "freeze";
    const unsigned hz = fixture.value("player_hz",20);
    const bool passed = c.entered && c.finished && !c.open && c.samples == tick * (hz/20) &&
        (hook ? c.motionChecks > 5 && c.travel > 150 :
         // The scripted three-turns/second gesture's canonical20 receipt awards
         // 35 points and releases at q372. Require those same world boundaries.
         ((c.analogStruggle ? c.analogProgress == 35 && c.worldCounts == 61 && c.releaseQ == 372 :
                             c.worldCounts == (freeze ? 96 : 20)) &&
          (hz == 20 || c.intermediateChecks > 0) &&
          (!freeze || (c.inputEdges == 1 && c.firstInputQ == (hz == 120 ? 31 : 36) && c.damagePulses > 0))));
    const json report = {{"status",passed ? "pass" : "fail"},{"mechanic",c.kind},{"player_hz",hz},
        {"samples",c.samples},{"entry_q",c.entryQ},{"release_q",c.releaseQ},{"exit_q",c.exitQ},
        {"world_counts",c.worldCounts},{"intermediate_checks",c.intermediateChecks},
        {"damage_pulses",c.damagePulses},{"input_edges",c.inputEdges},{"first_input_q",c.firstInputQ},
        {"analog_progress",c.analogProgress},
        {"motion_checks",c.motionChecks},{"checked_travel",c.travel},{"maximum_step_motion",c.maxMotion}};
    { std::ofstream f(output / "action-rate.json"); f << report.dump(2) << '\n';
      if (!f) Fail("action rate receipt write failed"); }
    if (!passed) Fail("action rate coverage incomplete: " + report.dump());
}

// Real world actors, with only their placement authored here. Actor_Spawn uses
// Shipwright's normal resource-backed object fallback outside map loading.
// No action, push accumulator, platform timer or motion is synthesized.
struct DynaChecks {
    std::string kind;
    DynaPolyActor* target = nullptr;
    Vec3f beforeActor{};
    float beforeIntent = 0, initialY = 0, firstHeight = 0;
    bool open = false, haveHeight = false;
    uint64_t worlds = 0, pushes = 0, pulls = 0, intermediates = 0, riders = 0;
    double maximumIntent = 0, maximumActorStep = 0, maximumRideDrift = 0;
} dynaChecks;

void SetupDynaChecks() {
    auto& c = dynaChecks; c = {};
    c.kind = fixture.value("test_dyna",std::string());
    if (c.kind.empty()) return;
    auto* p = GET_PLAYER(gPlayState);
    Vec3f pos = p->actor.world.pos;
    const bool lift = c.kind == "lift";
    if (lift) {
        pos.y += 40.0f;
    } else {
        pos.x += Math_SinS(p->yaw) * 80.0f;
        pos.z += Math_CosS(p->yaw) * 80.0f;
    }
    auto* actor = Actor_Spawn(&gPlayState->actorCtx,gPlayState,lift ? ACTOR_BG_HIDAN_FSLIFT : ACTOR_EN_AM,
        pos.x,pos.y,pos.z,0,0,0,0);
    if (!actor || !actor->update) Fail("dyna fixture actor did not spawn");
    c.target = reinterpret_cast<DynaPolyActor*>(actor); c.initialY = pos.y;
    if (lift) {
        if (!actor->child || !actor->child->update) Fail("real lift target child missing");
        p->actor.world.pos.y += 150.0f;
        p->actor.home.pos = p->actor.prevPos = p->actor.world.pos;
        p->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    }
}

void ObserveDyna(const char* site, PlayState* play, Actor* actor) {
    auto& c = dynaChecks;
    if (!enabled || !measuring || !c.target) return;
    auto require = [&](bool okay, const char* why) {
        if (!okay) Fail(c.kind + " coupling at tick" + std::to_string(tick) + ": " + why);
    };
    if (actor == &c.target->actor) {
        if (!std::strcmp(site,"actor.update.begin")) {
            c.beforeActor = actor->world.pos;
            const float intent = c.target->unk_150;
            c.maximumIntent = std::max(c.maximumIntent,double(std::fabs(intent)));
            require(std::fabs(intent) <= 2.001f,"world push intent multiplied");
            c.pushes += intent > 0; c.pulls += intent < 0;
        } else if (!std::strcmp(site,"actor.update.end")) {
            const double distance = Math_Vec3f_DistXYZ(&c.beforeActor,&actor->world.pos);
            c.maximumActorStep = std::max(c.maximumActorStep,distance); ++c.worlds;
            if (c.kind == "lift") require(distance <= 4.001,"lift world displacement changed");
        }
        return;
    }
    if (actor != &GET_PLAYER(play)->actor) return;
    if (!std::strcmp(site,"player_step.begin")) {
        c.beforeActor = c.target->actor.world.pos; c.beforeIntent = c.target->unk_150; c.open = true;
    } else if (!std::strcmp(site,"player_step.end")) {
        require(c.open,"unpaired intermediate observation"); c.open = false; ++c.intermediates;
        require(c.beforeIntent == c.target->unk_150 && Equal(c.beforeActor,c.target->actor.world.pos),
                "Player-only step mutated world actor intent or position");
    }
    if (!std::strcmp(site,"actor.update.end") || !std::strcmp(site,"player_step.end")) {
        auto* p = GET_PLAYER(play);
        if (c.kind == "lift" && p->actor.floorBgId == c.target->bgId && (p->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
            const float height = p->actor.world.pos.y - c.target->actor.world.pos.y;
            if (!c.haveHeight) { c.haveHeight = true; c.firstHeight = height; }
            c.maximumRideDrift = std::max(c.maximumRideDrift,double(std::fabs(height-c.firstHeight)));
            require(c.maximumRideDrift < 0.1,"Player drifted from the supporting lift"); ++c.riders;
        }
    }
}

void FinishDynaChecks() {
    const auto& c = dynaChecks;
    if (!c.target) return;
    const bool lift = c.kind == "lift";
    const bool passed = c.worlds == tick && !c.open && (lift ?
        c.riders > 30 && c.target->actor.world.pos.y-c.initialY > 80.0f : c.pushes > 5 && c.pulls > 5);
    const json report = {{"status",passed ? "pass" : "fail"},{"mechanic",c.kind},
        {"player_hz",fixture.value("player_hz",20)},{"world_updates",c.worlds},
        {"push_opportunities",c.pushes},{"pull_opportunities",c.pulls},{"maximum_intent",c.maximumIntent},
        {"maximum_actor_step",c.maximumActorStep},{"intermediate_checks",c.intermediates},
        {"rider_checks",c.riders},{"maximum_ride_drift",c.maximumRideDrift},
        {"rise",c.target->actor.world.pos.y-c.initialY}};
    { std::ofstream f(output / "dyna-coupling.json"); f << report.dump(2) << '\n'; }
    if (!passed) Fail("dyna coverage incomplete: " + report.dump());
}

// Seed only the owning actor's handoff or call the existing action entry.
// The engine owns animation, counter progress, attachments and release.
struct ActionClockChecks {
    std::string kind;
    bool open = false, world = false, active = false, entered = false, finished = false, manual = false, manualSeeded = false;
    int timer = 0;
    float frame = 0, speed = 0, length = 0;
    uint64_t changes = 0, held = 0, manualSamples = 0, highWaitSamples = 0;
    int attachment = 0;
    bool attached = false;
    uint64_t alignmentSamples = 0;
} actionClockChecks;

const char* ClockAction(const std::string& kind) {
    if (kind == "gallery") return "Player_Action_8084FA54";
    if (kind == "fishing") return "Player_Action_80850C68";
    if (kind == "heavy") return "Player_Action_80846120";
    if (kind == "lift") return "Player_Action_80846050";
    if (kind == "void") return "Player_Action_8084F88C";
    return "Player_Action_80846408";
}

void SetupActionClocks() {
    auto& c = actionClockChecks; c = {};
    c.kind = fixture.value("test_action_clock",std::string());
    if (c.kind.empty()) return;
    auto* p = GET_PLAYER(gPlayState);
    if (c.kind == "gallery") {
        gPlayState->shootingGalleryStatus = 16;
        if (!func_8084B3CC(gPlayState,p)) Fail("gallery owner handoff failed");
    } else if (c.kind == "fishing") {
        if (gPlayState->sceneNum != SCENE_FISHING_POND) Fail("fishing clock requires the pond interface");
        gPlayState->interfaceCtx.unk_260 = 1; // Owner starts play; the normal interface equips B.
        if (!Player_StartFishing(gPlayState)) Fail("fishing owner handoff failed");
    } else if (c.kind == "void") {
        p->stateFlags1 |= PLAYER_STATE1_FLOOR_DISABLED;
        gPlayState->nextEntranceIndex = gSaveContext.entranceIndex;
        if (!func_80838FB8(gPlayState,p)) Fail("void transition handoff failed");
    } else {
        const bool heavy = c.kind == "heavy";
        Inventory_ChangeUpgrade(UPG_STRENGTH,(heavy || c.kind=="lift") ? 3 : 0);
        Actor* prop = Actor_Spawn(&gPlayState->actorCtx,gPlayState,heavy ? ACTOR_BG_HEAVY_BLOCK : ACTOR_EN_KUSA,
            p->actor.world.pos.x,p->actor.world.pos.y,p->actor.world.pos.z-60.0f,0,c.kind=="lift" ? 0x4000 : 0,0,0);
        if (!prop || !prop->update) Fail("carry clock prop failed to spawn");
        p->interactRangeActor = prop;
        // Both original grab callers retain the offer while waiting for the lift animation.
        // Without this flag Player_UpdateCommon clears it before heavy pickup frame41.
        p->stateFlags1 |= PLAYER_STATE1_CARRYING_ACTOR;
        p->getItemId = GI_NONE;
        p->getItemEntry = GET_ITEM_NONE;
        Player_ZeroSpeedXZ(p);
        p->unk_6AD = 0;
        func_8083A0F4(gPlayState,p);
    }
}

void ObserveActionClocks(const char* site, PlayState* play) {
    auto& c = actionClockChecks;
    if (!enabled || !measuring || c.kind.empty()) return;
    const bool begin = !std::strcmp(site,"actor.update.begin") || !std::strcmp(site,"player_step.begin");
    const bool end = !std::strcmp(site,"actor.update.end") || !std::strcmp(site,"player_step.end");
    if (!begin && !end) return;
    auto* p = GET_PLAYER(play);
    auto active = [&]() { return !std::strcmp(NativeSimTest_PlayerActionName(p),ClockAction(c.kind)); };
    auto require = [&](bool okay, const char* why) {
        if (!okay) Fail(c.kind + " clock at tick" + std::to_string(tick) + ": " + why);
    };
    if (begin) {
        require(!c.open,"unpaired begin"); c.open = true;
        c.world = !std::strcmp(site,"actor.update.begin");
        if (c.kind == "heavy" && active()) {
            if (p->skelAnime.curFrame < 41.0f) {
                require((p->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) && p->interactRangeActor &&
                        p->interactRangeActor->id == ACTOR_BG_HEAVY_BLOCK && p->interactRangeActor->update,
                        "heavy pickup lost its original grab preconditions");
            } else if (p->skelAnime.curFrame < 229.0f) {
                require(p->heldActor && p->heldActor->id == ACTOR_BG_HEAVY_BLOCK && p->heldActor->update,
                        "heavy throw lost its held block");
            }
        }
        if (c.world && tick == 5 && c.kind == "gallery") {
            require(active(),"gallery entry missing");
            play->shootingGalleryStatus = -2; // Original gallery actor's closing notification.
        }
        if (c.world && c.kind == "fishing" && !c.manualSeeded && active() && p->av2.actionVar2 != 0) {
            p->unk_858 = 0.5f; p->unk_85C = -0.5f; // World fishing actor's lure-direction handoff.
            c.manualSeeded = true;
        }
        c.active = active(); c.entered |= c.active;
        c.attachment = p->unk_3BC.y; c.attached = p->heldActor != nullptr;
        c.timer = c.kind == "gallery" ? play->shootingGalleryStatus : p->av2.actionVar2;
        c.frame = p->skelAnime.curFrame; c.speed = p->skelAnime.playSpeed; c.length = p->skelAnime.animLength;
        c.manual = c.kind == "fishing" && c.active && c.timer && (p->unk_858 || p->unk_85C);
        return;
    }
    require(c.open,"unpaired end"); c.open = false;
    if (c.active) {
        if (c.manual) {
            const auto time = PlayerTemporal_Observe();
            float expected = c.frame + c.speed * (time.high ? (time.end-time.start) * 0.25f : R_UPDATE_RATE * 0.5f);
            if (expected >= c.length) expected -= c.length;
            require(active() && p->skelAnime.curFrame == expected,"manual animation advanced at the wrong rate");
            ++c.manualSamples;
        } else if (c.kind == "lift" && c.attached && p->heldActor && active()) {
            const auto time=PlayerTemporal_Observe();
            const float duration=time.high ? (time.end-time.start)/6.0f : 1.0f;
            require(std::abs(int16_t(p->unk_3BC.y-c.attachment)) <= 4000*(R_UPDATE_RATE*0.5f)*duration+1,
                    "held attachment alignment changed too fast");
            ++c.alignmentSamples;
        } else if (c.kind == "void") {
            require(p->av2.actionVar2 == c.timer+int(c.world),"void wait skipped world opportunity");
            require((play->transitionTrigger != TRANS_TRIGGER_OFF) == (c.timer+int(c.world)>9),
                    "void transition did not keep its original boundary");
            if (c.world) ++c.changes; else ++c.held;
            c.finished |= play->transitionTrigger != TRANS_TRIGGER_OFF;
        } else if (c.kind == "gallery" && c.timer < 0) {
            require(play->shootingGalleryStatus == c.timer + int(c.world),"gallery closing skipped world opportunity");
            if (c.world) ++c.changes; else ++c.held;
        } else if ((c.kind == "carry" && c.timer > 0) || (c.kind == "heavy" && c.timer > 0)) {
            if (PlayerTemporal_Observe().high) ++c.highWaitSamples;
            const bool remains = active();
            if (c.kind == "carry") {
                require(remains ? p->av2.actionVar2 == c.timer-int(c.world) : c.world && c.timer == 1,
                        "failed lift hold skipped world opportunity");
            } else {
                require(remains ? p->av2.actionVar2 == c.timer+int(c.world) : c.world && c.timer == 21,
                        "heavy throw recovery skipped world opportunity");
            }
            if (c.world) ++c.changes; else ++c.held;
        }
        if (!active()) c.finished = true;
    }
    c.entered |= active();
}

void FinishActionClocks() {
    const auto& c = actionClockChecks;
    if (c.kind.empty()) return;
    const bool pass = c.entered && !c.open && (c.kind == "fishing" ? c.manualSamples >= 30 :
        c.kind == "lift" ? c.finished && c.alignmentSamples>0 :
        c.finished && c.changes == (c.kind == "gallery" ? 2 : c.kind == "carry" ? 15 : c.kind=="void" ? 10 : 21));
    json report = {{"status",pass ? "pass" : "fail"},{"kind",c.kind},{"entered",c.entered},
        {"finished",c.finished},{"world_changes",c.changes},{"held_steps",c.held},
        {"manual_samples",c.manualSamples},{"high_wait_samples",c.highWaitSamples},{"alignment_samples",c.alignmentSamples}};
    { std::ofstream f(output / "action-clock.json"); f << report.dump(2) << '\n'; }
    if (!pass) Fail("action clock coverage incomplete: " + report.dump());
}

// Four bounded scenarios use real C-item input, swim entry and existing scene geometry.
struct MotionEffectChecks {
    std::string kind, action;
    bool open = false, world = false, bunny = false, slope = false;
    int timer = 0, divePhase = 0, tilt = 0;
    float speed = 0;
    uint64_t rng = 0, worlds = 0, held = 0, scoopSounds = 0, swimWait = 0, turns = 0, rises = 0, slides = 0;
    double maxSlideChange = 0;
    bool pushOpen = false;
    float pushBefore = 0, pushTarget = 0, pushStep = 0;
    uint64_t pushSamples = 0;
    int16_t yaw = 0, targetYaw = 0;
    bool grounded = false;
    uint64_t airSamples = 0;
    double maxAirBrake = 0, maxAirTurn = 0;
    uint64_t voidSurfaces = 0, voidTick = 0;
    float voidGap = 0;
    int voidPoly = -1;
} motionEffectChecks;

void SetupMotionEffects() {
    auto& c = motionEffectChecks; c = {};
    c.kind = fixture.value("test_motion_effect",std::string());
    if (c.kind.empty()) return;
    auto* play = gPlayState; auto* p = GET_PLAYER(play);
    if (c.kind == "bunny" || c.kind == "scoop") {
        gSaveContext.equips.buttonItems[1] = c.kind == "bunny" ? ITEM_MASK_BUNNY : ITEM_BOTTLE;
        const int slot = c.kind == "bunny" ? SLOT_TRADE_CHILD : SLOT_BOTTLE_1;
        gSaveContext.inventory.items[slot] = gSaveContext.equips.buttonItems[1];
        gSaveContext.equips.cButtonSlots[0] = slot;
    } else if (c.kind == "swim") {
        p->actor.world.pos.y -= 60.0f;
        p->actor.prevPos = p->actor.world.pos;
        if (Player_SetStartingMovement(play,p,180.0f)) Fail("swim entry requires deep water");
        Inventory_ChangeUpgrade(UPG_SCALE,2);
    } else if (c.kind == "air") {
        // Controlled drop over the ordinary Kokiri fixture ground. Enter the
        // real falling action, leaving landing, motion and collision to the game.
        p->actor.world.pos.y += 200.0f;
        p->actor.prevPos = p->actor.world.pos;
        p->actor.bgCheckFlags &= ~(BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH);
        func_80837B9C(p,play);
        p->actor.shape.rot.y=0; p->yaw=0x2000; p->linearVelocity=6.0f;
        p->actor.velocity={Math_SinS(p->yaw)*6.0f,0,Math_CosS(p->yaw)*6.0f};
        Camera_InitPlayerSettings(GET_ACTIVE_CAM(play),p);
    } else if (c.kind == "slope" || c.kind == "uphill") {
        if (play->transitionTrigger != TRANS_TRIGGER_OFF) Fail("slope setup began during a scene transition");
        auto* header = play->colCtx.colHeader;
        CollisionPoly* best = nullptr; Vec3f position{}; float largest = 0;
        for (int i=0; i<header->numPolygons; ++i) {
            auto* poly = &header->polyList[i];
            const float ny = COLPOLY_GET_NORMAL(poly->normal.y);
            if (ny < 0.5f || ny > 0.95f || SurfaceType_GetFloorEffect(&play->colCtx,poly,BGCHECK_SCENE) != FLOOR_EFFECT_1 ||
                SurfaceType_GetSceneExitIndex(&play->colCtx,poly,BGCHECK_SCENE) != 0 ||
                func_80041EA4(&play->colCtx,poly,BGCHECK_SCENE) == FLOOR_PROPERTY_5 ||
                func_80041EA4(&play->colCtx,poly,BGCHECK_SCENE) == FLOOR_PROPERTY_12) continue;
            Vec3f v[3]; CollisionPoly_GetVerticesByBgId(poly,BGCHECK_SCENE,&play->colCtx,v);
            float area = std::fabs((v[1].x-v[0].x)*(v[2].z-v[0].z)-(v[2].x-v[0].x)*(v[1].z-v[0].z));
            if (area > largest) {
                largest=area; best=poly;
                position={(v[0].x+v[1].x+v[2].x)/3,(v[0].y+v[1].y+v[2].y)/3,(v[0].z+v[1].z+v[2].z)/3};
            }
        }
        if (!best) Fail("scene has no suitable real slippery slope");
        func_8083C0E8(p,play); // Controlled ordinary idle handoff before entering the actual slope handler.
        p->actor.world.pos = p->actor.prevPos = p->actor.home.pos = position;
        p->actor.floorPoly=best; p->actor.floorBgId=BGCHECK_SCENE; p->actor.floorHeight=position.y;
        p->actor.bgCheckFlags |= BGCHECKFLAG_GROUND;
        p->yaw=p->actor.shape.rot.y=p->actor.world.rot.y=Math_Atan2S(best->normal.z,best->normal.x) +
            (c.kind=="uphill" ? 0x8000 : 0);
        p->linearVelocity=c.kind=="uphill" ? 6.0f : 2.0f;
        p->actor.velocity={Math_SinS(p->yaw)*p->linearVelocity,0,Math_CosS(p->yaw)*p->linearVelocity};
        if (c.kind=="slope" && !Player_HandleSlopes(play,p,best)) Fail("real slope did not admit sliding: action="+
            std::string(NativeSimTest_PlayerActionName(p))+" flags="+std::to_string(p->stateFlags1)+
            " cs="+std::to_string(p->csAction));
        Camera_InitPlayerSettings(GET_ACTIVE_CAM(play),p);
        if(c.kind=="uphill") GET_ACTIVE_CAM(play)->inputDir.y=p->yaw;
    }
}

void ObserveMotionEffects(const char* site, PlayState* play) {
    auto& c = motionEffectChecks;
    if (!enabled || !measuring || c.kind.empty()) return;
    if (!std::strcmp(site,"player.void_surface") && fixture.value("check_slope_void",false)) {
        auto* p=GET_PLAYER(play);
        if (!PlayerTemporal_WorldOpportunity(p) || !p->actor.floorPoly ||
            p->actor.floorBgId!=BGCHECK_SCENE || p->floorProperty!=FLOOR_PROPERTY_5 ||
            !(p->actor.bgCheckFlags&BGCHECKFLAG_GROUND) ||
            p->actor.world.pos.y-p->actor.floorHeight>=100.0f)
            Fail("slope void did not originate from a grounded real static respawn surface");
        ++c.voidSurfaces; c.voidTick=tick;
        c.voidGap=p->actor.world.pos.y-p->actor.floorHeight;
        c.voidPoly=int(p->actor.floorPoly-play->colCtx.colHeader->polyList);
        return;
    }
    if (c.kind=="uphill" && !std::strncmp(site,"slope.push.",11)) {
        auto* p=GET_PLAYER(play);
        if (!std::strcmp(site,"slope.push.begin")) {
            if(c.pushOpen || !p->actor.floorPoly) Fail("invalid uphill observation");
            c.pushOpen=true; c.pushBefore=p->pushedSpeed;
            c.pushTarget=(1.0f-COLPOLY_GET_NORMAL(p->actor.floorPoly->normal.y))*40.0f;
            c.pushStep=std::max(c.pushTarget*c.pushTarget*0.015f,1.2f);
        } else {
            if(!c.pushOpen) Fail("unpaired uphill end");
            const auto time=PlayerTemporal_Observe();
            const float duration=time.high ? (time.end-time.start)/6.0f : 1.0f;
            const float distance=c.pushTarget-c.pushBefore;
            const float expected=c.pushBefore+std::copysign(std::min(std::fabs(distance),c.pushStep*duration),distance);
            if(std::fabs(p->pushedSpeed-expected)>0.00001f) Fail("uphill force ignored elapsed Player duration");
            c.pushOpen=false; ++c.pushSamples;
        }
        return;
    }
    const bool begin = !std::strcmp(site,"actor.update.begin") || !std::strcmp(site,"player_step.begin");
    const bool end = !std::strcmp(site,"actor.update.end") || !std::strcmp(site,"player_step.end");
    if (!begin && !end) return;
    auto* p = GET_PLAYER(play);
    const std::string action = NativeSimTest_PlayerActionName(p);
    auto require = [&](bool okay,const char* why) { if (!okay) Fail(c.kind+" at tick"+std::to_string(tick)+": "+why); };
    if (begin) {
        require(!c.open,"unpaired begin"); c.open=true; c.world=!std::strcmp(site,"actor.update.begin");
        c.action=action; c.timer=p->av2.actionVar2; c.divePhase=p->av1.actionVar1; c.tilt=p->unk_6C2;
        c.speed=p->linearVelocity; c.rng=streams["gameplay"].calls; c.bunny=p->currentMask==PLAYER_MASK_BUNNY;
        c.yaw=p->yaw; c.targetYaw=p->actor.shape.rot.y; c.grounded=p->actor.bgCheckFlags&BGCHECKFLAG_GROUND;
        c.slope=p->actor.floorPoly && SurfaceType_GetFloorEffect(&play->colCtx,p->actor.floorPoly,p->actor.floorBgId)==FLOOR_EFFECT_1;
        return;
    }
    require(c.open,"unpaired end"); c.open=false;
    const auto time=PlayerTemporal_Observe(); const float duration=time.high ? (time.end-time.start)/6.0f : 1.0f;
    if((c.kind=="slope" || c.kind=="air") && c.action=="Player_Action_8084411C" && action==c.action && !c.grounded) {
        // Neutral input in this real slope-to-fall fixture: ordinary air control
        // brakes toward zero and turns toward the held shape yaw. The initial
        // jump-speed clamp is a constraint, not a rate-dependent decrement.
        require(!p->focusActor && !(p->stateFlags2&PLAYER_STATE2_HOPPING),"unsupported air-check profile");
        const float before=std::min(c.speed,R_RUN_SPEED_LIMIT/100.0f);
        const bool reversing=std::abs(int16_t(c.yaw-c.targetYaw))>0x6000;
        const float brake=(reversing ? 1.0f : 0.1f)*duration;
        require(before>=0 && std::fabs(p->linearVelocity-std::max(0.0f,before-brake))<0.00001f,
                "air braking ignored elapsed duration");
        const int turn=std::abs(int16_t(p->yaw-c.yaw));
        require(reversing ? (p->linearVelocity==0 ? p->yaw==c.targetYaw : turn==0) :
                turn<=200*(R_UPDATE_RATE*0.5f)*duration+1,"air steering changed too fast");
        c.maxAirBrake=std::max(c.maxAirBrake,double(before-p->linearVelocity));
        c.maxAirTurn=std::max(c.maxAirTurn,double(turn)); ++c.airSamples;
    }
    if (c.kind=="bunny" && c.bunny && p->currentMask==PLAYER_MASK_BUNNY) {
        const auto calls=streams["gameplay"].calls-c.rng;
        require(c.world ? calls>=2 : calls==0,"cosmetic RNG escaped world ownership");
        if(c.world) ++c.worlds; else ++c.held;
    }
    if(c.action=="Player_Action_8084D7C4" && c.timer>0) {
        require(action==c.action ? p->av2.actionVar2==c.timer-int(c.world) : c.world && c.timer==1,
                "swim entry wait skipped world opportunity");
        if(c.world) ++c.swimWait;
    }
    if(c.action=="Player_Action_8084DAB4") ++c.turns;
    if(c.action=="Player_Action_8084DC48" && c.divePhase>=2 && action==c.action) {
        require(std::abs(int16_t(p->unk_6C2-c.tilt)) <= 1200*(R_UPDATE_RATE*0.5f)*duration+2,
                "surfacing pitch changed too fast");
        ++c.rises;
    }
    if(c.action=="Player_Action_SlideOnSlope" && action==c.action && c.slope && p->actor.floorPoly &&
       SurfaceType_GetFloorEffect(&play->colCtx,p->actor.floorPoly,p->actor.floorBgId)==FLOOR_EFFECT_1) {
        const double change=std::fabs(p->linearVelocity-c.speed);
        require(change<=1.5*duration+0.001,"slope acceleration changed too fast");
        c.maxSlideChange=std::max(c.maxSlideChange,change); ++c.slides;
    }
}

void FinishMotionEffects() {
    const auto& c=motionEffectChecks;
    if(c.kind.empty()) return;
    if(fixture.value("check_slope_void",false) &&
       (c.voidSurfaces!=1 || gPlayState->transitionTrigger==TRANS_TRIGGER_OFF))
        Fail("slope did not make exactly one valid respawn handoff");
    const auto& finalPosition=GET_PLAYER(gPlayState)->actor.world.pos;
    // This checks the force operation, not sustained ascent: this steep real
    // face legitimately changes to sliding after the initial uphill approach.
    const bool pass=!c.open && !c.pushOpen && (c.kind=="uphill" ? c.pushSamples>0 :
        c.kind=="bunny" ? c.worlds>=25 : c.kind=="scoop" ? c.scoopSounds==1 :
        c.kind=="air" ? c.airSamples>=10 && (GET_PLAYER(gPlayState)->actor.bgCheckFlags&BGCHECKFLAG_GROUND) :
        c.kind=="slope" ? c.slides>=10 && c.airSamples>=5 : c.swimWait==20 && c.turns>0 && c.rises>0);
    json report={{"status",pass?"pass":"fail"},{"kind",c.kind},{"worlds",c.worlds},{"intermediate_holds",c.held},
        {"scoop_sounds",c.scoopSounds},{"swim_wait",c.swimWait},{"turn_samples",c.turns},{"rise_samples",c.rises},
        {"slope_samples",c.slides},{"max_slope_speed_change",c.maxSlideChange},{"uphill_samples",c.pushSamples},
        {"air_samples",c.airSamples},{"max_air_braking",c.maxAirBrake},{"max_air_turn",c.maxAirTurn},
        {"final_position",{finalPosition.x,finalPosition.y,finalPosition.z}},
        {"final_action",NativeSimTest_PlayerActionName(GET_PLAYER(gPlayState))},
        {"void_surfaces",c.voidSurfaces},{"void_tick",c.voidTick},{"void_polygon",c.voidPoly},{"void_gap",c.voidGap}};
    {std::ofstream f(output/"motion-effect.json");f<<report.dump(2)<<'\n';}
    if(!pass) Fail("motion/effect coverage incomplete: "+report.dump());
}

struct SpellWaitChecks {
    std::string kind;
    bool open = false, world = false, active = false, entered = false, finished = false;
    int phase = 0, timer = 0, initialMagic = 0;
    uint64_t positive = 0, negative = 0, held = 0, spawns = 0, entryQ = 0, exitQ = 0;
} spellWaitChecks;

void ObserveSpellWait(const char* site, PlayState* play) {
    auto& c = spellWaitChecks;
    if (!enabled || !measuring || c.kind.empty()) return;
    const bool begin = !std::strcmp(site,"actor.update.begin") || !std::strcmp(site,"player_step.begin");
    const bool end = !std::strcmp(site,"actor.update.end") || !std::strcmp(site,"player_step.end");
    if (!begin && !end) return;
    auto* p = GET_PLAYER(play);
    const bool active = !std::strcmp(NativeSimTest_PlayerActionName(p),"Player_Action_808507F4");
    const auto time = PlayerTemporal_Observe();
    const uint64_t q = tick * 6 + (time.high ? time.start % 6 : 0);
    auto require = [&](bool okay, const char* reason) {
        if (!okay) Fail(c.kind + " spell at q" + std::to_string(q) + ": " + reason);
    };
    if (begin) {
        require(!c.open,"unpaired begin");
        c.open = true; c.world = !std::strcmp(site,"actor.update.begin");
        c.active = active; c.phase = p->av1.actionVar1; c.timer = p->av2.actionVar2;
        return;
    }
    require(c.open,"unpaired end"); c.open = false;
    if (!c.entered && active) {
        c.entered = true; c.entryQ = q;
        require(p->av1.actionVar1 == (c.kind == "farore" ? 0 : 2),"wrong spell selected");
    }
    if (c.active && c.phase >= 0) {
        if (c.timer < 0) {
            const int expected = c.timer == -1 && c.world ? 2 : c.timer + int(c.world);
            require(active && p->av2.actionVar2 == expected,"respawn wait cadence");
            if (c.world) ++c.negative; else ++c.held;
            if (expected == 2) require(gSaveContext.fw.set == 1 &&
                gSaveContext.respawn[RESPAWN_MODE_TOP].data == 1,"warp point not recorded");
        } else if (c.timer >= 2) {
            const int limit = c.kind == "farore" ? 70 : 10;
            require(active && p->av2.actionVar2 == c.timer + int(c.world),"spell hold cadence");
            require(p->av1.actionVar1 == (c.world && c.timer > limit ? -1 : c.phase),"spell release threshold");
            if (c.world) ++c.positive; else ++c.held;
        } else if (c.timer == 0 && active && p->av2.actionVar2 == 1) {
            int actors = 0;
            for (int category = 0; category < ACTORCAT_MAX; ++category)
                for (Actor* a = play->actorCtx.actorLists[category].head; a; a = a->next)
                    actors += a->update && a->id == (c.kind == "farore" ? ACTOR_MAGIC_WIND : ACTOR_MAGIC_FIRE);
            require(actors == 1,"spell actor missing or duplicated"); ++c.spawns;
        }
    }
    if (c.active && !active) { c.finished = true; c.exitQ = q; }
}

void FinishSpellWait() {
    const auto& c = spellWaitChecks;
    if (c.kind.empty()) return;
    const bool farore = c.kind == "farore";
    const bool passed = c.entered && c.finished && !c.open && c.spawns == 1 &&
        c.positive == (farore ? 70 : 10) && c.negative == (farore ? 9 : 0) &&
        gSaveContext.magic == c.initialMagic - 12; // Farore and Din; Nayru costs24.
    const json receipt = {{"status",passed ? "pass" : "fail"},{"spell",c.kind},
        {"player_hz",fixture.value("player_hz",20)},{"entry_q",c.entryQ},{"exit_q",c.exitQ},
        {"world_hold_increments",c.positive},{"world_respawn_increments",c.negative},
        {"intermediate_holds",c.held},{"spell_spawns",c.spawns},
        {"magic_consumed",c.initialMagic-gSaveContext.magic}};
    { std::ofstream f(output / "spell-wait.json"); f << receipt.dump(2) << '\n';
      if (!f) Fail("spell receipt write failed"); }
    if (!passed) Fail("spell wait coverage incomplete: " + receipt.dump());
}

// Real small-hit entry and real soil/item exchange. Observe the action-owned
// timers online; no action/timer assignment or full substep trace is needed.
struct ActionWaitChecks {
    std::string kind;
    Actor* bean = nullptr;
    bool open = false, world = false, entered = false, finished = false, beforeAction = false;
    int beforeTimer = 0, initialHealth = 0, beforeHealth = 0, initialAmmo = 0;
    uint64_t decrements = 0, heldSteps = 0, damageEvents = 0, enterQ = 0, exitQ = 0;
    uint64_t firstDecrementQ = UINT64_MAX, lastDecrementQ = 0, samples = 0;
} actionWaitChecks;

Actor* FindBean(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_BG].head; actor; actor = actor->next)
        if (actor->id == ACTOR_OBJ_BEAN && actor->update) return actor;
    Fail("bean fixture requires the scene's live soil actor");
}

void ObserveActionWait(const char* site, PlayState* play) {
    auto& c = actionWaitChecks;
    if (!enabled || !measuring || c.kind.empty()) return;
    const bool begin = !std::strcmp(site, "actor.update.begin") || !std::strcmp(site, "player_step.begin");
    const bool end = !std::strcmp(site, "actor.update.end") || !std::strcmp(site, "player_step.end");
    if (!begin && !end) return;
    auto* p = GET_PLAYER(play);
    const auto time = PlayerTemporal_Observe();
    const uint64_t q = tick * 6 + (time.high ? time.start % 6 : 0);
    const bool bean = c.kind == "bean";
    auto active = [&]() { return !std::strcmp(NativeSimTest_PlayerActionName(p),
        bean ? "Player_Action_8084279C" : "Player_Action_8084377C"); };
    auto require = [&](bool okay, const char* why) {
        if (!okay) Fail(c.kind + " action wait at q" + std::to_string(q) + ": " + why);
    };
    if (begin) {
        require(!c.open, "unpaired begin");
        c.world = !std::strcmp(site, "actor.update.begin");
        if (!bean && c.world && tick == 2) {
            require((p->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !p->invincibilityTimer,
                    "small-hit fixture did not start grounded and vulnerable");
            // Same public request used by actors. Ordinary Player damage handling
            // chooses the action, launches Link, and sets its own recovery timer.
            Actor_SetPlayerKnockbackSmall(play, nullptr, 3.0f, p->yaw, 6.0f, 4);
        }
        c.beforeAction = active();
        c.beforeTimer = p->av2.actionVar2;
        c.beforeHealth = gSaveContext.health;
        c.open = true;
        return;
    }
    require(c.open, "unpaired end");
    ++c.samples;
    if (gSaveContext.health < c.beforeHealth) ++c.damageEvents;
    if (!c.entered && active()) {
        c.entered = true; c.enterQ = q;
        require(p->av2.actionVar2 == (bean ? 80 : 4), "entry did not author the expected timer");
    }
    if (c.beforeAction && !c.finished) {
        const bool remains = active();
        const int after = remains ? p->av2.actionVar2 : 0;
        require(after == c.beforeTimer || after == c.beforeTimer - 1, "timer skipped a count");
        if (after < c.beforeTimer) {
            require(c.world, "countdown advanced on a Player-only step");
            if (c.firstDecrementQ == UINT64_MAX) c.firstDecrementQ = q;
            else require(q - c.lastDecrementQ == 6, "grounded countdown skipped a world opportunity");
            c.lastDecrementQ = q; ++c.decrements;
        } else if (!c.world) ++c.heldSteps;
        if (!remains) {
            require(c.beforeTimer == 1 && c.world, "action released before its countdown");
            c.finished = true; c.exitQ = q;
        }
    }
    c.open = false;
}

void FinishActionWait() {
    const auto& c = actionWaitChecks;
    if (c.kind.empty()) return;
    const bool bean = c.kind == "bean";
    const unsigned hz = fixture.value("player_hz", 20);
    const int ammoUsed = bean ? c.initialAmmo - AMMO(ITEM_BEAN) : 0;
    const bool planted = bean && FindBean(gPlayState) == c.bean &&
        Flags_GetSwitch(gPlayState, c.bean->params & 0x3f);
    const bool passed = !c.open && c.entered && c.finished && c.decrements == (bean ? 80 : 4) &&
        c.samples == tick * (hz / 20) && (hz == 20 || c.heldSteps > 0) &&
        (bean ? planted && ammoUsed == 1 && c.damageEvents == 0 :
                c.damageEvents == 1 && gSaveContext.health < c.initialHealth);
    const json report = {{"status",passed ? "pass" : "fail"},{"mechanic",c.kind},{"player_hz",hz},
        {"samples",c.samples},{"entry_q",c.enterQ},{"exit_q",c.exitQ},
        {"first_countdown_q",c.firstDecrementQ},{"last_countdown_q",c.lastDecrementQ},
        {"world_countdowns",c.decrements},{"held_intermediate_steps",c.heldSteps},
        {"damage_events",c.damageEvents},{"health_delta",gSaveContext.health-c.initialHealth},
        {"beans_used",ammoUsed},{"bean_planted",planted}};
    { std::ofstream file(output / "action-wait.json"); file << report.dump(2) << '\n';
      if (!file) Fail("action wait receipt write failed"); }
    if (!passed) Fail("action wait coverage incomplete: " + report.dump());
}

// Two bounded item-lifetime fixtures; all countdowns, actions and collision
// queries execute through the ordinary Player update, never a helper replay.
struct PlayerItemTimerChecks {
    std::string kind;
    bool open = false, world = false, armed = false, expired = false, landed = false, itemRemoved = false;
    int beforeTimer = 0, beforeAmmo = 0, initialAmmo = 0;
    float beforeScale = 0, startY = 0, minY = 0, maxY = 0;
    float hoverHoldMinY = 0, hoverHoldMaxY = 0;
    uint64_t hoverHoldSamples = 0;
    const char* hoverBeforeAction = nullptr;
    float hoverBeforeX = 0, hoverBeforeZ = 0, hoverRunDistance = 0;
    uint64_t hoverRunStarts = 0, hoverRunStops = 0;
    bool hoverSlewEligible = false;
    float hoverBeforeSpeed = 0, hoverSlewCap = 0, hoverMaxSpeedDelta = 0;
    uint64_t hoverSlewChecks = 0, hoverSlewChanges = 0;
    uint64_t worlds = 0, intermediates = 0, countdowns = 0, expiries = 0, ammoUsed = 0;
    uint64_t groundedRefreshes = 0, expiryTick = UINT64_MAX, landingTick = UINT64_MAX, postExpiryWorlds = 0;
} playerItemTimerChecks;

void ObservePlayerItemTimer(const char* site, PlayState* play) {
    auto& check = playerItemTimerChecks;
    if (!enabled || !measuring || check.kind.empty()) return;
    const bool worldBegin = !std::strcmp(site, "actor.update.begin");
    const bool intermediateBegin = !std::strcmp(site, "player_step.begin");
    const bool worldEnd = !std::strcmp(site, "actor.update.end");
    const bool intermediateEnd = !std::strcmp(site, "player_step.end");
    if (!worldBegin && !intermediateBegin && !worldEnd && !intermediateEnd) return;
    auto* player = GET_PLAYER(play);
    const bool hover = check.kind == "hover";
    auto require = [&](bool condition, const std::string& detail) {
        if (!condition) Fail(check.kind + " timer at " + site + ": " + detail);
    };
    if (worldBegin || intermediateBegin) {
        require(!check.open, "missing previous end observation");
        if (hover) {
            require(player->currentBoots == PLAYER_BOOTS_HOVER, "Hover Boots are not equipped");
            if (worldBegin && tick == 2) {
                require((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
                            player->actor.floorPoly && player->actor.floorBgId == BGCHECK_SCENE &&
                            player->hoverBootsTimer == 19,
                        "airborne setup requires the real grounded19 refresh on static floor");
                // A short drop above the already settled static floor. Preserve
                // the production timer and action; normal collision owns landing.
                player->actor.world.pos.y += 80.0f;
                player->actor.prevPos = player->actor.home.pos = player->actor.world.pos;
                player->actor.velocity.y = 0.0f;
                player->fallStartHeight = static_cast<int16_t>(player->actor.world.pos.y);
                player->actor.bgCheckFlags &= ~(BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH |
                                               BGCHECKFLAG_GROUND_LEAVE | BGCHECKFLAG_GROUND_STRICT);
                check.startY = check.minY = check.maxY = player->actor.world.pos.y;
                check.armed = true;
            }
            check.beforeTimer = player->hoverBootsTimer;
            check.hoverBeforeAction = NativeSimTest_PlayerActionName(player);
            check.hoverBeforeX = player->actor.world.pos.x;
            check.hoverBeforeZ = player->actor.world.pos.z;
            // This airborne fixture has no impulses; the actual hover branch
            // remains eligible through the expiry step before collision runs.
            check.hoverSlewEligible = check.armed && !check.expired &&
                !(player->skelAnime.movementFlags & 0x80) &&
                !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_IN_CUTSCENE));
            check.hoverBeforeSpeed = player->actor.speedXZ;
            const unsigned quanta = PlayerTemporal_HighStepQuanta(player);
            check.hoverSlewCap = 0.35f * (quanta ? quanta / 6.0f : 1.0f);
        } else {
            if (worldBegin && tick == 20) {
                require(player->heldItemAction == PLAYER_IA_DEKU_STICK && player->itemAction == PLAYER_IA_DEKU_STICK &&
                            player->unk_85C == 1.0f && player->unk_860 == 0 && AMMO(ITEM_STICK) >= 2,
                        "real C-item input did not equip an intact, unlit stick");
                // Same initial lifetime as an actual torch ignition; the fixture
                // does not replace the item action or manually expire the stick.
                player->unk_860 = 210;
                check.initialAmmo = AMMO(ITEM_STICK);
                check.armed = true;
            }
            check.beforeTimer = player->unk_860;
            check.beforeScale = player->unk_85C;
            check.beforeAmmo = AMMO(ITEM_STICK);
            if (check.armed && !check.expired)
                require(player->heldItemAction == PLAYER_IA_DEKU_STICK && check.beforeTimer > 0 && check.beforeScale > 0,
                        "burning stick disappeared before its lifetime expired");
        }
        check.world = worldBegin;
        check.open = true;
        return;
    }

    require(check.open && check.world == worldEnd, "unpaired item observation");
    if (check.world) ++check.worlds;
    else ++check.intermediates;
    if (hover) {
        if (check.hoverSlewEligible) {
            const float delta = std::fabs(player->actor.speedXZ - check.hoverBeforeSpeed);
            require(delta <= check.hoverSlewCap + 0.00001f,
                    "speedXZ slew exceeded per-step cap: delta " + std::to_string(delta) +
                        ", cap " + std::to_string(check.hoverSlewCap));
            check.hoverMaxSpeedDelta = std::max(check.hoverMaxSpeedDelta, delta);
            ++check.hoverSlewChecks;
            check.hoverSlewChanges += delta > 0.000001f;
        }
        if (!check.armed) {
            require((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && player->hoverBootsTimer == 19,
                    "ordinary grounded update did not refresh19");
            ++check.groundedRefreshes;
        } else if (!check.expired) {
            const int expected = check.beforeTimer - (check.world ? 1 : 0);
            require(!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND), "landed before hover countdown completed");
            require(player->hoverBootsTimer == expected, "expected " + std::to_string(expected) +
                        ", got " + std::to_string(player->hoverBootsTimer));
            check.minY = std::min(check.minY, player->actor.world.pos.y);
            check.maxY = std::max(check.maxY, player->actor.world.pos.y);
            if (expected > 0) {
                // Entry precedes hover detection; compare the sustained hold
                // against its first real airborne endpoint, not the injected Y.
                if (check.hoverHoldSamples++ == 0)
                    check.hoverHoldMinY = check.hoverHoldMaxY = player->actor.world.pos.y;
                check.hoverHoldMinY = std::min(check.hoverHoldMinY, player->actor.world.pos.y);
                check.hoverHoldMaxY = std::max(check.hoverHoldMaxY, player->actor.world.pos.y);
                require(check.hoverHoldMaxY - check.hoverHoldMinY <= 0.001f,
                        "sustained hover height drift: min " + std::to_string(check.hoverHoldMinY) +
                            ", max " + std::to_string(check.hoverHoldMaxY));
                const char* afterAction = NativeSimTest_PlayerActionName(player);
                const bool wasRun = !std::strcmp(check.hoverBeforeAction, "Player_Action_80842180");
                const bool isRun = !std::strcmp(afterAction, "Player_Action_80842180");
                check.hoverRunStarts += !std::strcmp(check.hoverBeforeAction, "Player_Action_Idle") && isRun;
                check.hoverRunStops += wasRun && !std::strcmp(afterAction, "Player_Action_Idle");
                if (wasRun || isRun) {
                    const float dx = player->actor.world.pos.x - check.hoverBeforeX;
                    const float dz = player->actor.world.pos.z - check.hoverBeforeZ;
                    check.hoverRunDistance += std::sqrt(dx * dx + dz * dz);
                }
            }
            if (check.world) ++check.countdowns;
            if (expected == 0) {
                check.expired = true; ++check.expiries; check.expiryTick = tick;
            }
        } else if (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
            require(player->hoverBootsTimer == 19, "landing did not refresh19");
            if (!check.landed) check.landingTick = tick;
            check.landed = true;
            ++check.groundedRefreshes;
        }
    } else if (check.armed && !check.expired) {
        if (!check.world) {
            require(player->unk_860 == check.beforeTimer && player->unk_85C == check.beforeScale &&
                        AMMO(ITEM_STICK) == check.beforeAmmo,
                    "intermediate update advanced stick lifetime, scale or ammo");
        } else {
            ++check.countdowns;
            const bool expires = check.beforeTimer == 1;
            const int expectedTimer = expires ? 1 : check.beforeTimer - 1;
            const int expectedAmmo = check.beforeAmmo - (expires ? 1 : 0);
            require(player->unk_860 == expectedTimer && AMMO(ITEM_STICK) == expectedAmmo,
                    "world countdown/ammo mismatch: timer " + std::to_string(player->unk_860) +
                        ", ammo " + std::to_string(AMMO(ITEM_STICK)));
            if (expires) {
                require(player->unk_85C == 0.0f, "expired stick did not reach zero scale");
                check.expired = true; ++check.expiries; ++check.ammoUsed; check.expiryTick = tick;
            }
        }
    } else if (check.expired) {
        require(AMMO(ITEM_STICK) == check.initialAmmo - 1, "stick ammo changed again after expiry");
        check.itemRemoved |= player->heldItemAction != PLAYER_IA_DEKU_STICK;
        check.postExpiryWorlds += check.world;
    }
    check.open = false;
}

void FinishPlayerItemTimerChecks() {
    const auto& check = playerItemTimerChecks;
    if (check.kind.empty()) return;
    const unsigned hz = fixture.value("player_hz", 20);
    const bool coverage = !check.open && check.armed && check.expired && check.expiries == 1 &&
        check.worlds == tick && check.intermediates == tick * (hz / 20 - 1);
    const bool passed = coverage && (check.kind == "hover" ?
        check.countdowns == 19 && check.expiryTick == 20 && check.landed && check.groundedRefreshes > 2 &&
            check.hoverHoldSamples == 18 * (hz / 20) && check.hoverRunStarts >= 1 &&
            check.hoverRunStops >= 1 && check.hoverRunDistance > 1.0f &&
            check.hoverSlewChecks == check.hoverHoldSamples + 1 && check.hoverSlewChanges > 0 :
        check.countdowns == 210 && check.expiryTick == 229 && check.ammoUsed == 1 &&
            check.itemRemoved && check.postExpiryWorlds >= 2);
    const json report = {{"status", passed ? "pass" : "fail"}, {"mechanic", check.kind}, {"player_hz", hz},
        {"world_checks", check.worlds}, {"intermediate_checks", check.intermediates},
        {"countdown_opportunities", check.countdowns}, {"expiries", check.expiries}, {"expiry_tick", check.expiryTick},
        {"grounded_refreshes", check.groundedRefreshes}, {"landing_tick", check.landingTick},
        {"hover_start_y", check.startY}, {"hover_min_y", check.minY}, {"hover_max_y", check.maxY},
        {"hover_hold_samples", check.hoverHoldSamples}, {"hover_hold_min_y", check.hoverHoldMinY},
        {"hover_hold_max_y", check.hoverHoldMaxY},
        {"hover_run_starts", check.hoverRunStarts}, {"hover_run_stops", check.hoverRunStops},
        {"hover_run_distance_xz", check.hoverRunDistance},
        {"hover_slew_checks", check.hoverSlewChecks}, {"hover_slew_changes", check.hoverSlewChanges},
        {"hover_max_speed_delta", check.hoverMaxSpeedDelta},
        {"ammo_used", check.ammoUsed}, {"item_removed", check.itemRemoved},
        {"post_expiry_world_checks", check.postExpiryWorlds}};
    { std::ofstream file(output / "player-item-timer.json"); file << report.dump(2) << '\n';
      if (!file) Fail("Player item timer receipt write failed"); }
    if (!passed) Fail("Player item timer coverage incomplete: " + report.dump());
}

// Observe the scene's real locked handle door; never assign an action, timer,
// key, switch or room state. The fixture only positions Player during setup.
struct DoorLockChecks {
    Actor* actor = nullptr;
    Player* player = nullptr;
    PlayState* play = nullptr;
    EnDoorActionFunc idleAction = nullptr;
    bool playerOpen = false, actorOpen = false, world = false, accepted = false;
    bool beforeOpening = false, beforeActorIdle = false;
    int beforePlayerTimer = 0, beforeLockTimer = 0, beforeKeys = 0, initialKeys = 0;
    unsigned beforePressed = 0;
    uint64_t sampleQ = 0, acceptedQ = UINT64_MAX, playerUnlockedQ = UINT64_MAX, actorUnlockedQ = UINT64_MAX;
    uint64_t worlds = 0, intermediates = 0, actorWorlds = 0, playerDecrements = 0, actorDecrements = 0;
    uint64_t starts = 0, keysSpent = 0, boundaryChecks = 0;
} doorLockChecks;

EnDoor* DoorLockTarget(PlayState* play) {
    const auto& check = doorLockChecks;
    if (!play || play != check.play || play->sceneNum != SCENE_FIRE_TEMPLE ||
        GET_PLAYER(play) != check.player || gSaveContext.mapIndex != SCENE_FIRE_TEMPLE)
        Fail("door lock fixture lost its Fire Temple/Player context");
    // Compare retained identity before following it; a room/actor lifetime change
    // fails through the ordinary test reporter rather than dereferencing residue.
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_DOOR].head; actor; actor = actor->next) {
        if (actor != check.actor) continue;
        if (actor->id != ACTOR_EN_DOOR || !actor->update || (static_cast<u16>(actor->params) & 0x3ff) != 0x97)
            Fail("door lock fixture target changed identity or lock type");
        return reinterpret_cast<EnDoor*>(actor);
    }
    Fail("door lock fixture target is no longer a live room door");
}

void SetupDoorLockChecks() {
    if (!fixture.value("test_door_lock", false)) return;
    doorLockChecks = {};
    auto& check = doorLockChecks;
    auto* play = gPlayState;
    if (!play || play->sceneNum != SCENE_FIRE_TEMPLE || play->roomCtx.curRoom.num != 0 ||
        gSaveContext.mapIndex != SCENE_FIRE_TEMPLE || !GET_PLAYER(play))
        Fail("door lock fixture requires the ordinary Fire Temple entrance room");
    check.play = play;
    check.player = GET_PLAYER(play);
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_DOOR].head; actor; actor = actor->next) {
        if (actor->id == ACTOR_EN_DOOR && actor->world.pos.x == 280.0f &&
            actor->world.pos.y == 0.0f && actor->world.pos.z == 140.0f) {
            if (check.actor) Fail("door lock fixture has an ambiguous scene door");
            check.actor = actor;
        }
    }
    auto* door = DoorLockTarget(play);
    auto* player = check.player;
    Vec3f relative;
    Actor_WorldToActorCoords(&door->actor, &relative, &player->actor.world.pos);
    check.initialKeys = gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex];
    if ((static_cast<u16>(door->actor.params) >> 10) != 34 || door->actor.shape.rot.y != 0 ||
        door->lockTimer != 10 || door->playerIsOpening || Flags_GetSwitch(play, 0x17) ||
        check.initialKeys != 8 || std::fabs(relative.x) >= 1.0f || std::fabs(relative.y) >= 1.0f ||
        relative.z <= 20.0f || relative.z >= 50.0f || player->actor.shape.rot.y != -32768 ||
        !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || player->doorActor != &door->actor ||
        player->doorType != PLAYER_DOORTYPE_HANDLE || player->doorTimer != 10 ||
        Player_InCsMode(play) || !door->actionFunc) {
        const json actual = {
            {"transition_index", static_cast<u16>(door->actor.params) >> 10},
            {"door_yaw", door->actor.shape.rot.y}, {"lock_timer", door->lockTimer},
            {"opening", door->playerIsOpening != 0}, {"switch_0x17", Flags_GetSwitch(play, 0x17) != 0},
            {"keys", check.initialKeys}, {"relative_pos", {relative.x, relative.y, relative.z}},
            {"player_pos", {player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z}},
            {"player_yaw", player->actor.shape.rot.y}, {"bg_flags", player->actor.bgCheckFlags},
            {"grounded", (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0},
            {"offered_target", player->doorActor == &door->actor}, {"door_type", player->doorType},
            {"expected_door_type", PLAYER_DOORTYPE_HANDLE}, {"player_timer", player->doorTimer},
            {"in_cs_mode", Player_InCsMode(play) != 0}, {"door_action_present", door->actionFunc != nullptr},
            {"player_action", NativeSimTest_PlayerActionName(player)}, {"player_speed", player->linearVelocity},
            {"player_state1", player->stateFlags1}, {"setup_frames", setupFrames}
        };
        Fail("door lock fixture did not settle at the real locked door offer with eight debug-save keys: " +
             actual.dump());
    }
    check.idleAction = door->actionFunc;
}

void ObserveDoorLockPlayer(const char* site, PlayState* play) {
    if (!enabled || !measuring || !fixture.value("test_door_lock", false)) return;
    const bool worldBegin = !std::strcmp(site, "actor.update.begin");
    const bool stepBegin = !std::strcmp(site, "player_step.begin");
    const bool worldEnd = !std::strcmp(site, "actor.update.end");
    const bool stepEnd = !std::strcmp(site, "player_step.end");
    if (!worldBegin && !stepBegin && !worldEnd && !stepEnd) return;
    auto& check = doorLockChecks;
    auto* door = DoorLockTarget(play);
    auto* player = check.player;
    if (worldBegin || stepBegin) {
        if (check.playerOpen) Fail("door fixture missed a Player end observation");
        check.playerOpen = true;
        check.world = worldBegin;
        const auto temporal = PlayerTemporal_Observe();
        check.sampleQ = tick * 6 + (worldBegin ? 0 : temporal.start % 6);
        check.beforePlayerTimer = player->doorTimer;
        check.beforeOpening = door->playerIsOpening != 0;
        check.beforePressed = worldBegin ? play->state.input[0].press.button : temporal.pressed;
        return;
    }
    if (!check.playerOpen || check.world != worldEnd) Fail("door fixture has unmatched Player observations");
    if (!check.beforeOpening && door->playerIsOpening) {
        const uint64_t expectedQ = fixture.value("player_hz", 20) == 120 ? 7 : 12;
        if (check.accepted || !(check.beforePressed & BTN_A) || check.sampleQ != expectedQ ||
            std::strcmp(NativeSimTest_PlayerActionName(player), "Player_Action_80845EF8") ||
            player->doorActor != &door->actor || player->doorTimer != 10 || player->skelAnime.endFrame != 0.0f)
            Fail("door fixture did not start the ordinary locked-door action on its A input opportunity");
        check.accepted = true;
        check.acceptedQ = check.sampleQ;
        ++check.starts;
    }
    if (player->doorTimer != check.beforePlayerTimer) {
        if (!check.accepted || !check.world || check.beforePlayerTimer <= 0 ||
            player->doorTimer != check.beforePlayerTimer - 1)
            Fail("Player door countdown changed outside one world decrement");
        ++check.playerDecrements;
    }
    const bool waitReleased = check.accepted && player->av2.actionVar2 != 0 && player->skelAnime.endFrame > 0.0f;
    if (waitReleased && check.playerUnlockedQ == UINT64_MAX) check.playerUnlockedQ = check.sampleQ;
    // A world Player end precedes the paired door update, so synchronization is
    // checked at the full frame below. Intermediate steps have no such producer.
    if (stepEnd && waitReleased && door->lockTimer != 0)
        Fail("intermediate Player released the door wait before its world-owned lock");
    check.world ? ++check.worlds : ++check.intermediates;
    check.playerOpen = false;
}

void ObserveDoorLockActor(const char* site, PlayState* play, Actor* actor) {
    if (!enabled || !measuring || !fixture.value("test_door_lock", false) || actor != doorLockChecks.actor) return;
    auto& check = doorLockChecks;
    auto* door = DoorLockTarget(play);
    if (!std::strcmp(site, "actor.update.begin")) {
        if (check.actorOpen) Fail("door fixture missed a world actor end observation");
        check.actorOpen = true;
        check.beforeLockTimer = door->lockTimer;
        check.beforeKeys = gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex];
        check.beforeActorIdle = door->actionFunc == check.idleAction;
    } else if (!std::strcmp(site, "actor.update.end")) {
        if (!check.actorOpen) Fail("door fixture missed a world actor begin observation");
        const int expected = check.beforeLockTimer - (!check.beforeActorIdle && check.beforeLockTimer > 0);
        if (door->lockTimer != expected) Fail("EnDoor lock countdown did not retain its ordinary world opportunity");
        if (door->lockTimer != check.beforeLockTimer) {
            ++check.actorDecrements;
            if (!door->lockTimer) check.actorUnlockedQ = tick * 6;
        }
        const int spent = check.beforeKeys - gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex];
        if (spent) {
            if (spent != 1 || !check.accepted || check.keysSpent || !check.beforeActorIdle ||
                !Flags_GetSwitch(play, 0x17)) Fail("door fixture did not consume exactly one key through EnDoor");
            ++check.keysSpent;
        }
        ++check.actorWorlds;
        check.actorOpen = false;
    }
}

void ObserveDoorLockBoundary(PlayState* play) {
    if (!fixture.value("test_door_lock", false)) return;
    auto* door = DoorLockTarget(play);
    auto& check = doorLockChecks;
    if (check.playerOpen || check.actorOpen) Fail("door fixture reached a boundary with an open observation");
    if (check.playerUnlockedQ != UINT64_MAX && door->lockTimer != 0)
        Fail("Player released the door wait before the lock at full world boundary q" +
             std::to_string(tick * 6) + "; remaining lock ticks " + std::to_string(door->lockTimer));
    ++check.boundaryChecks;
}

void FinishDoorLockChecks() {
    if (!fixture.value("test_door_lock", false)) return;
    auto* door = DoorLockTarget(gPlayState);
    const auto& check = doorLockChecks;
    const unsigned hz = fixture.value("player_hz", 20);
    const bool passed = !check.playerOpen && !check.actorOpen && check.starts == 1 && check.keysSpent == 1 &&
        check.playerDecrements == 10 && check.actorDecrements == 10 && check.worlds == tick &&
        check.intermediates == tick * (hz / 20 - 1) && check.actorWorlds == tick && check.boundaryChecks == tick &&
        check.playerUnlockedQ != UINT64_MAX && check.actorUnlockedQ != UINT64_MAX &&
        check.playerUnlockedQ >= check.actorUnlockedQ && check.player->doorTimer == 0 && door->lockTimer == 0 &&
        gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex] == check.initialKeys - 1 &&
        Flags_GetSwitch(gPlayState, 0x17) && check.player->skelAnime.curFrame > 0.0f;
    const json report = {{"status", passed ? "pass" : "fail"}, {"player_hz", hz},
        {"accepted_q", check.acceptedQ}, {"player_unlocked_q", check.playerUnlockedQ},
        {"actor_unlocked_q", check.actorUnlockedQ}, {"starts", check.starts}, {"keys_spent", check.keysSpent},
        {"world_checks", check.worlds}, {"intermediate_checks", check.intermediates},
        {"actor_world_checks", check.actorWorlds}, {"boundary_checks", check.boundaryChecks},
        {"player_decrements", check.playerDecrements}, {"actor_decrements", check.actorDecrements}};
    { std::ofstream file(output / "door-lock.json"); file << report.dump(2) << '\n';
      if (!file) Fail("door lock receipt write failed"); }
    if (!passed) Fail("door lock coverage incomplete: " + report.dump());
}

json Float(float value) {
    if (!std::isfinite(value)) Fail("non-finite semantic float");
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return {{"bits", Hex(bits, 8)}, {"value", value}};
}
json Vec(const Vec3f& value) { return {{"x", Float(value.x)}, {"y", Float(value.y)}, {"z", Float(value.z)}}; }
json Rot(const Vec3s& value) { return {{"x", value.x}, {"y", value.y}, {"z", value.z}}; }
json ActorId(Actor* actor) {
    if (!actor) return nullptr;
    auto it = actorIds.find(actor);
    if (it == actorIds.end()) return "untracked";
    return "scene" + std::to_string(sceneEpoch) + ":spawn" + std::to_string(it->second);
}
bool AuditCanonicalContacts() {
    return enabled && measuring && NativeSimTest_ConfigInt("player_hz",20) == 20;
}
void AuditContactRecord(json event) {
    event["tick"] = tick; event["phase"] = phase; event["actor"] = ActorId(currentActor);
    // Hash a small typed event directly, never serialize a phase snapshot.
    for (unsigned char byte : NativeValidation::Hash(event))
        phaseContactOrder = (phaseContactOrder ^ byte) * 1099511628211ull;
    ++phaseContactEvents;
}
void AuditContactSample(const char* site, Player* p) {
    if (!AuditCanonicalContacts()) return;
    AuditContactRecord({{"kind","player_sample"},{"site",site},{"melee_state",p->meleeWeaponState},
        {"melee_animation",p->meleeWeaponAnimation},{"combo",p->unk_845}});
}
json Polygon(CollisionPoly* poly) {
    if (!poly) return nullptr;
    // Semantic geometry, not an address or allocator-dependent polygon ordinal.
    return {{"type", poly->type}, {"vertices", {poly->flags_vIA, poly->flags_vIB, poly->vIC}},
            {"normal", Rot(poly->normal)}, {"dist", poly->dist}};
}
json ActorState(Actor* actor) {
    return {{"identity", ActorId(actor)}, {"type", actor->id}, {"params", actor->params},
            {"category", actor->category}, {"room", actor->room}, {"flags", actor->flags},
            {"position", Vec(actor->world.pos)}, {"previous_position", Vec(actor->prevPos)},
            {"rotation", Rot(actor->world.rot)}, {"shape_rotation", Rot(actor->shape.rot)},
            {"focus", Vec(actor->focus.pos)}, {"velocity", Vec(actor->velocity)},
            {"speed_xz", Float(actor->speedXZ)}, {"gravity", Float(actor->gravity)},
            {"min_velocity_y", Float(actor->minVelocityY)}, {"scale", Vec(actor->scale)},
            {"bg_flags", actor->bgCheckFlags}, {"floor_height", Float(actor->floorHeight)},
            {"floor_bg_id", actor->floorBgId}, {"wall_bg_id", actor->wallBgId},
            {"floor", Polygon(actor->floorPoly)}, {"wall", Polygon(actor->wallPoly)},
            {"wall_yaw", actor->wallYaw}, {"health", actor->colChkInfo.health},
            {"damage", actor->colChkInfo.damage}, {"displacement", Vec(actor->colChkInfo.displacement)},
            {"freeze_timer", actor->freezeTimer}, {"color_filter_timer", actor->colorFilterTimer},
            {"is_drawn", actor->isDrawn}, {"initializing", actor->init != nullptr},
            {"alive", actor->update != nullptr}, {"has_draw", actor->draw != nullptr},
            {"parent", ActorId(actor->parent)}, {"child", ActorId(actor->child)}, {"sfx", actor->sfx}};
}
json Animation(SkelAnime& animation) {
    std::string resource = "unmapped";
    if (animation.animation && ResourceMgr_OTRSigCheck(static_cast<char*>(animation.animation)))
        resource = static_cast<const char*>(animation.animation);
    return {{"resource", resource}, {"mode", animation.mode}, {"frame", Float(animation.curFrame)},
            {"start", Float(animation.startFrame)}, {"end", Float(animation.endFrame)},
            {"length", Float(animation.animLength)}, {"speed", Float(animation.playSpeed)},
            {"morph_weight", Float(animation.morphWeight)}, {"morph_rate", Float(animation.morphRate)},
            {"movement_flags", animation.movementFlags}, {"previous_translation", Rot(animation.prevTransl)},
            {"previous_rotation", animation.prevRot}};
}
json PlayerState(Player* player) {
    json state = ActorState(&player->actor);
    state["action"] = NativeSimTest_PlayerActionName(player);
    state["animation"] = Animation(player->skelAnime);
    state["upper_animation"] = Animation(player->upperSkelAnime);
    state["linear_velocity"] = Float(player->linearVelocity);
    state["yaw"] = player->yaw;
    state["state_flags"] = {player->stateFlags1, player->stateFlags2, player->stateFlags3};
    state["action_variables"] = {player->av1.actionVar1, player->av2.actionVar2};
    state["invincibility_timer"] = player->invincibilityTimer;
    state["target_timer"] = player->zTargetActiveTimer;
    state["target"] = ActorId(player->focusActor);
    state["held_actor"] = ActorId(player->heldActor);
    state["door_timer"] = player->doorTimer;
    state["fall_distance"] = player->fallDistance;
    state["fall_start_height"] = player->fallStartHeight;
    state["melee_state"] = player->meleeWeaponState;
    state["melee_animation"] = player->meleeWeaponAnimation;
    state["cs_action"] = player->csAction;
    state["damage_flicker"] = player->damageFlickerAnimCounter;
    state["left_hand"] = Vec(player->leftHandPos);
    state["body_parts"] = json::array();
    for (auto& pos : player->bodyPartsPos) state["body_parts"].push_back(Vec(pos));
    state["weapon_geometry"] = json::array();
    for (auto& weapon : player->meleeWeaponInfo)
        state["weapon_geometry"].push_back({{"active", weapon.active}, {"tip", Vec(weapon.tip)}, {"base", Vec(weapon.base)}});
    state["shield_quad"] = json::array();
    for (auto& pos : player->shieldQuad.dim.quad) state["shield_quad"].push_back(Vec(pos));
    return state;
}
json ColliderState(const Collider& c, PlayState* play) {
    json state = {{"actor", ActorId(c.actor)}, {"at_actor", ActorId(c.at)}, {"ac_actor", ActorId(c.ac)},
        {"oc_actor", ActorId(c.oc)}, {"at_flags", c.atFlags}, {"ac_flags", c.acFlags},
        {"oc_flags", {c.ocFlags1, c.ocFlags2}}, {"shape", c.shape}, {"material", c.colType}};
    auto indices = [&](Collider** list, int count) {
        json result = json::array();
        for (int i = 0; i < count; ++i) if (list[i] == &c) result.push_back(i);
        return result;
    };
    state["registered_at"] = indices(play->colChkCtx.colAT, play->colChkCtx.colATCount);
    state["registered_ac"] = indices(play->colChkCtx.colAC, play->colChkCtx.colACCount);
    state["registered_oc"] = indices(play->colChkCtx.colOC, play->colChkCtx.colOCCount);
    return state;
}
json ColliderElement(const ColliderInfo& info) {
    return {{"touch_flags", info.toucherFlags}, {"bump_flags", info.bumperFlags}, {"oc_flags", info.ocElemFlags},
        {"damage_flags", info.toucher.dmgFlags}, {"damage", info.toucher.damage}, {"effect", info.toucher.effect},
        {"accept_flags", info.bumper.dmgFlags}, {"defense", info.bumper.defense},
        {"hit_position", Rot(info.bumper.hitPos)}};
}
json QuadState(const ColliderQuad& quad, PlayState* play) {
    auto state = ColliderState(quad.base, play);
    state["element"] = ColliderElement(quad.info);
    state["vertices"] = json::array();
    for (const auto& v : quad.dim.quad) state["vertices"].push_back(Vec(v));
    state["nearest_distance"] = Float(quad.dim.acDist);
    return state;
}
json MatrixState(const MtxF& matrix) {
    json result = json::array();
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) result.push_back(Float(matrix.mf[i][j]));
    return result;
}
json PlayerDetail(PlayState* play) {
    auto* p = GET_PLAYER(play);
    if (!p) return nullptr;
    json state = {{"pose_generation", playerPoseGeneration}, {"contact_count", playerContacts},
        {"combo_count", p->unk_845}, {"combo_timer", p->unk_844},
        {"held_item_action", p->heldItemAction}, {"item_action", p->itemAction},
        {"model_group", p->modelGroup}, {"hand_types", {p->leftHandType, p->rightHandType}},
        {"shield", p->currentShield}, {"parallel_yaw", p->parallelYaw}, {"head_rotation", Rot(p->headLimbRot)},
        {"upper_rotation", Rot(p->upperLimbRot)}, {"upper_yaw", p->upperLimbYawSecondary},
        {"root_tilt", p->unk_6C2}, {"root_offset", Float(p->unk_6C4)},
        {"attachment_rotation", Rot(p->unk_3BC)}, {"hookshot_position", Vec(p->unk_3C8)},
        {"left_hand_matrix", MatrixState(p->mf_9E0)}, {"shield_matrix", MatrixState(p->shieldMf)},
        {"previous_waist", Vec(p->unk_A88)}, {"feet", {Vec(p->actor.shape.feetPos[0]), Vec(p->actor.shape.feetPos[1])}},
        {"floor_pitch", p->floorPitch}, {"floor_pitch_alt", p->floorPitchAlt}, {"floor_property", p->floorProperty},
        {"previous_floor_type", p->prevFloorType}, {"floor_timer", p->floorTypeTimer},
        {"ledge_height", Float(p->yDistToLedge)}, {"wall_distance", Float(p->distToInteractWall)},
        {"ledge_type", p->ledgeClimbType}, {"ledge_timer", p->ledgeClimbDelayTimer},
        {"stick_history_index", p->controlStickDataIndex}, {"stick_directions", p->controlStickDirections},
        {"stick_spin_angles", p->controlStickSpinAngles}, {"previous_stick_angle", p->prevControlStickAngle},
        {"previous_stick_magnitude", Float(p->prevControlStickMagnitude)},
        {"textbox_cooldown", p->textboxBtnCooldownTimer}, {"pushed_speed", Float(p->pushedSpeed)},
        {"pushed_yaw", p->pushedYaw}, {"sword_quads", {QuadState(p->meleeWeaponQuads[0], play), QuadState(p->meleeWeaponQuads[1], play)}},
        {"shield_quad", QuadState(p->shieldQuad, play)}};
    state["cylinder"] = ColliderState(p->cylinder.base, play);
    state["cylinder"]["element"] = ColliderElement(p->cylinder.info);
    state["cylinder"]["position"] = Rot(p->cylinder.dim.pos);
    state["cylinder"]["height"] = p->cylinder.dim.height;
    state["cylinder"]["radius"] = p->cylinder.dim.radius;
    state["cylinder"]["y_shift"] = p->cylinder.dim.yShift;
    state["joints"] = json::array(); state["morph_joints"] = json::array();
    state["upper_joints"] = json::array(); state["upper_morph_joints"] = json::array();
    for (int i = 0; i < PLAYER_LIMB_BUF_COUNT; ++i) {
        state["joints"].push_back(Rot(p->jointTable[i]));
        state["morph_joints"].push_back(Rot(p->morphTable[i]));
        state["upper_joints"].push_back(Rot(p->upperJointTable[i]));
        state["upper_morph_joints"].push_back(Rot(p->upperMorphTable[i]));
    }
    state["signs"] = json::array();
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_PROP].head; a; a = a->next) {
        if (a->id != ACTOR_EN_KANBAN || a->init || !a->update) continue;
        auto* sign = reinterpret_cast<EnKanban*>(a);
        json target = {{"identity", ActorId(a)}, {"position", Vec(a->world.pos)},
            {"action", sign->actionState}, {"parts", sign->partFlags}, {"invincibility", sign->invincibilityTimer},
            {"cut_type", sign->cutType}, {"piece_type", sign->pieceType}, {"damage", a->colChkInfo.damage}};
        // Fragments do not initialize or register the sign cylinder.
        if (a->params != ENKANBAN_PIECE) {
            target["collider"] = ColliderState(sign->collider.base, play);
            target["element"] = ColliderElement(sign->collider.info);
        }
        state["signs"].push_back(std::move(target));
    }
    return state;
}
json CameraState(Camera* cam) {
    if (!cam) return nullptr;
    return {{"eye", Vec(cam->eye)}, {"at", Vec(cam->at)}, {"up", Vec(cam->up)},
            {"eye_next", Vec(cam->eyeNext)}, {"fov", Float(cam->fov)}, {"distance", Float(cam->dist)},
            {"xz_speed", Float(cam->xzSpeed)}, {"speed_ratio", Float(cam->speedRatio)},
            {"pos_offset", Vec(cam->posOffset)}, {"player_delta", Vec(cam->playerPosDelta)},
            {"input_direction", Rot(cam->inputDir)}, {"direction", Rot(cam->camDir)},
            {"mode", cam->mode}, {"setting", cam->setting}, {"status", cam->status},
            {"anim_state", cam->animState}, {"timer", cam->timer}, {"target", ActorId(cam->target)},
            {"at_lerp", Float(cam->atLERPStepScale)}, {"yaw_rate_inv", Float(cam->yawUpdateRateInv)},
            {"pitch_rate_inv", Float(cam->pitchUpdateRateInv)}, {"r_rate_inv", Float(cam->rUpdateRateInv)},
            {"fov_rate", Float(cam->fovUpdateRate)}, {"xz_offset_rate", Float(cam->xzOffsetUpdateRate)},
            {"y_offset_rate", Float(cam->yOffsetUpdateRate)}};
}
json DrawState(PlayState* play) {
    NativeSimHudObservation h = {};
    NativeSimMessageObservation m = {};
    Interface_GetNativeSimHudObservation(play, &h);
    Message_GetNativeSimObservation(play, &m);
    json hud, message;
    // Explicit fields only: never serialize POD padding, pointers, or resource text.
#define HUD_FIELD(name) hud[#name] = h.name
    HUD_FIELD(main_next_second); HUD_FIELD(main_state_timer);
    HUD_FIELD(sub_next_second); HUD_FIELD(sub_state_timer);
    HUD_FIELD(digits); HUD_FIELD(timer_x); HUD_FIELD(timer_y);
    HUD_FIELD(env_hazard); HUD_FIELD(env_hazard_active);
    HUD_FIELD(game_mode); HUD_FIELD(no_ui); HUD_FIELD(pause_state); HUD_FIELD(pause_debug_state);
    HUD_FIELD(game_over_state); HUD_FIELD(message_mode); HUD_FIELD(message_length);
    HUD_FIELD(player_state_flags1); HUD_FIELD(player_state_flags2); HUD_FIELD(player_state_flags3);
    HUD_FIELD(player_cs_action); HUD_FIELD(player_unk_6ad); HUD_FIELD(player_item_action); HUD_FIELD(magic_state);
    HUD_FIELD(transition_trigger); HUD_FIELD(transition_mode); HUD_FIELD(cutscene_state);
    HUD_FIELD(in_cutscene_mode); HUD_FIELD(minigame_state); HUD_FIELD(shooting_gallery_status);
    HUD_FIELD(scene); HUD_FIELD(bowling_switch_38); HUD_FIELD(timer_gate_open); HUD_FIELD(countdown_length_gate_open);
    HUD_FIELD(draw_debug_mode); HUD_FIELD(draw_world_enabled); HUD_FIELD(draw_overlay_enabled);
    HUD_FIELD(pause_menu_mode); HUD_FIELD(transition_unknown_state);
    HUD_FIELD(c_up_timer); HUD_FIELD(c_up_invisible); HUD_FIELD(counter_digits);
    HUD_FIELD(do_action_state); HUD_FIELD(do_action_current); HUD_FIELD(do_action_next);
    hud["do_action_rotation"] = Float(h.do_action_rotation);
    HUD_FIELD(navi_calling); HUD_FIELD(a_alpha); HUD_FIELD(b_alpha); HUD_FIELD(health_alpha);
    HUD_FIELD(magic_alpha); HUD_FIELD(screen_fill_alpha);
    hud["health_capacity"] = gSaveContext.healthCapacity;
#undef HUD_FIELD
    json hudPaint;
#define HUD_PAINT_FIELD(name) hudPaint[#name] = h.paint.name
    HUD_PAINT_FIELD(observed); HUD_PAINT_FIELD(draw_frame); HUD_PAINT_FIELD(clock_count);
    HUD_PAINT_FIELD(digit_count); HUD_PAINT_FIELD(timer_id); HUD_PAINT_FIELD(clock_x); HUD_PAINT_FIELD(clock_y);
    HUD_PAINT_FIELD(clock_width); HUD_PAINT_FIELD(clock_height); HUD_PAINT_FIELD(clock_s); HUD_PAINT_FIELD(clock_t);
    HUD_PAINT_FIELD(digit_values); HUD_PAINT_FIELD(digit_x); HUD_PAINT_FIELD(digit_y);
    HUD_PAINT_FIELD(digit_width); HUD_PAINT_FIELD(digit_height); HUD_PAINT_FIELD(digit_s); HUD_PAINT_FIELD(digit_t);
    HUD_PAINT_FIELD(digit_r); HUD_PAINT_FIELD(digit_g); HUD_PAINT_FIELD(digit_b); HUD_PAINT_FIELD(digit_a);
#undef HUD_PAINT_FIELD
    hud["paint"] = std::move(hudPaint);
#define MESSAGE_FIELD(name) message[#name] = m.name
    MESSAGE_FIELD(textId); MESSAGE_FIELD(choiceTextId); MESSAGE_FIELD(msgLength); MESSAGE_FIELD(msgMode);
    MESSAGE_FIELD(derivedState); MESSAGE_FIELD(textBoxProperties); MESSAGE_FIELD(textBoxType); MESSAGE_FIELD(textBoxPos);
    MESSAGE_FIELD(msgBufPos); MESSAGE_FIELD(textDrawPos); MESSAGE_FIELD(decodedTextLen); MESSAGE_FIELD(textUnskippable);
    MESSAGE_FIELD(textDelay); MESSAGE_FIELD(textDelayTimer); MESSAGE_FIELD(stateTimer); MESSAGE_FIELD(textboxEndType);
    MESSAGE_FIELD(choiceIndex); MESSAGE_FIELD(choiceNum); MESSAGE_FIELD(textPosX); MESSAGE_FIELD(textPosY);
    MESSAGE_FIELD(textColorR); MESSAGE_FIELD(textColorG); MESSAGE_FIELD(textColorB); MESSAGE_FIELD(textColorAlpha);
    MESSAGE_FIELD(textboxColorRed); MESSAGE_FIELD(textboxColorGreen); MESSAGE_FIELD(textboxColorBlue);
    MESSAGE_FIELD(textboxColorAlphaCurrent); MESSAGE_FIELD(textboxColorAlphaTarget);
    MESSAGE_FIELD(ocarinaMode); MESSAGE_FIELD(ocarinaAction); MESSAGE_FIELD(hasTalkActor);
    MESSAGE_FIELD(startFrameCount); MESSAGE_FIELD(textboxSkipped); MESSAGE_FIELD(nextTextId); MESSAGE_FIELD(textBoxNum);
    MESSAGE_FIELD(textFade); MESSAGE_FIELD(textIsCredits); MESSAGE_FIELD(messageHasSetSfx); MESSAGE_FIELD(lastPlayedSong);
    MESSAGE_FIELD(lastLanguage); MESSAGE_FIELD(displayAsEnglish); MESSAGE_FIELD(language);
    MESSAGE_FIELD(textboxX); MESSAGE_FIELD(textboxY); MESSAGE_FIELD(textboxWidth); MESSAGE_FIELD(textboxHeight);
    MESSAGE_FIELD(textboxTexWidth); MESSAGE_FIELD(textboxTexHeight); MESSAGE_FIELD(textboxXTarget); MESSAGE_FIELD(textboxYTarget);
    MESSAGE_FIELD(textboxWidthTarget); MESSAGE_FIELD(textboxHeightTarget);
    MESSAGE_FIELD(textboxTexWidthTarget); MESSAGE_FIELD(textboxTexHeightTarget);
    MESSAGE_FIELD(textboxEndX); MESSAGE_FIELD(textboxEndY); MESSAGE_FIELD(textInitX); MESSAGE_FIELD(textInitY);
    MESSAGE_FIELD(textLineSpacing); MESSAGE_FIELD(textCharScale); MESSAGE_FIELD(yreg15); MESSAGE_FIELD(yreg31);
    MESSAGE_FIELD(actionState); MESSAGE_FIELD(actionCurrent); MESSAGE_FIELD(actionTarget);
    MESSAGE_FIELD(actionOverride); MESSAGE_FIELD(actionOverrideId);
    message["actionRotation"] = Float(m.actionRotation);
    MESSAGE_FIELD(hudVisibilityMode); MESSAGE_FIELD(prevHudVisibilityMode);
    MESSAGE_FIELD(rawBufferValid); MESSAGE_FIELD(decodedBufferValid);
    message["rawBufferFingerprint"] = Hex(m.rawBufferFingerprint, 16);
    message["decodedBufferFingerprint"] = Hex(m.decodedBufferFingerprint, 16);
#undef MESSAGE_FIELD
    json paint;
#define PAINT_FIELD(name) paint[#name] = m.paint.name
    PAINT_FIELD(observed); PAINT_FIELD(entryMode); PAINT_FIELD(entryTextDrawPos); PAINT_FIELD(entryEndType);
    PAINT_FIELD(entryIconBranch); PAINT_FIELD(asciiPathComplete); PAINT_FIELD(displayListLinked);
    PAINT_FIELD(glyphCount); PAINT_FIELD(iconCount); PAINT_FIELD(iconType); PAINT_FIELD(iconX); PAINT_FIELD(iconY);
    PAINT_FIELD(drawFrame);
    paint["glyphFingerprint"] = Hex(m.paint.glyphFingerprint, 16);
    paint["iconFingerprint"] = Hex(m.paint.iconFingerprint, 16);
#undef PAINT_FIELD
    message["paint"] = std::move(paint);
    return {{"schema", 1}, {"hud", std::move(hud)}, {"message", std::move(message)},
            {"settings", {{"text_speed", CVarGetInteger(CVAR_ENHANCEMENT("TextSpeed"), 1)},
                          {"slow_text_speed", CVarGetInteger(CVAR_ENHANCEMENT("SlowTextSpeed"),
                                                           CVarGetInteger(CVAR_ENHANCEMENT("TextSpeed"), 1))},
                          {"skip_text", CVarGetInteger(CVAR_ENHANCEMENT("SkipText"), 0)},
                          {"text_spacing", CVarGetInteger(CVAR_ENHANCEMENT("TextSpacing"), 6)}}}};
}
json State(PlayState* play) {
    json rng = json::object();
    for (const auto& [name, stream] : streams)
        rng[name] = {{"calls", stream.calls}, {"draw_calls", stream.drawCalls},
                     {"state", stream.state}, {"order", Hex(stream.order, 16)}};
    json state = {{"schema", 1}, {"tick", tick}, {"time_q", tick * 6},
                  {"rng", rng}, {"audio", {{"blocks", audioBlocks}, {"samples", audioSamples},
                   {"clock_calls", audioClockCalls}, {"random", gAudioContext.audioRandom},
                   {"task_count", gAudioContext.totalTaskCnt}}}};
    state["audio"]["reset_status"] = static_cast<uint8_t>(gAudioContext.resetStatus);
    state["audio"]["reset_timer"] = static_cast<uint32_t>(gAudioContext.resetTimer);
    state["audio"]["command_write_position"] = gAudioContext.cmdWrPos;
    state["audio"]["command_read_position"] = gAudioContext.cmdRdPos;
    state["audio"]["memory_notes"] = json::array();
    for (const auto& note : sOcarinaSongNotes[OCARINA_SONG_MEMORY_GAME]) {
        state["audio"]["memory_notes"].push_back({{"pitch", note.pitch}, {"length", note.length},
            {"volume", note.volume}, {"vibrato", note.vibrato}, {"bend", note.bend}, {"b_flat_4", note.bFlat4Flag}});
    }
    state["audio"]["sequence_players"] = json::array();
    for (const auto& sequencePlayer : gAudioContext.seqPlayers) {
        state["audio"]["sequence_players"].push_back({
            {"enabled", static_cast<bool>(sequencePlayer.enabled)},
            {"finished", static_cast<bool>(sequencePlayer.finished)},
            {"muted", static_cast<bool>(sequencePlayer.muted)}, {"state", sequencePlayer.state},
            {"sequence", sequencePlayer.seqId}, {"tempo", sequencePlayer.tempo},
            {"tempo_accumulator", sequencePlayer.tempoAcc}, {"delay", sequencePlayer.delay},
            {"fade_timer", sequencePlayer.fadeTimer}, {"script_counter", sequencePlayer.scriptCounter},
            {"skip_ticks", sequencePlayer.skipTicks}, {"fade_volume", Float(sequencePlayer.fadeVolume)},
            {"volume", Float(sequencePlayer.volume)}, {"script_io", sequencePlayer.soundScriptIO}});
    }
    if (!play) return state;
    // Opt-in extension keeps the complete Pass 2 snapshot/trace contract unchanged.
    if (fixture.value("observe_draw_state", false)) state["draw_state"] = DrawState(play);
    state["global"] = {{"scene", play->sceneNum}, {"room", play->roomCtx.curRoom.num},
        {"entrance", gSaveContext.entranceIndex}, {"scene_epoch", sceneEpoch}, {"gameplay_frames", play->gameplayFrames},
        {"state_frames", play->state.frames}, {"day_time", gSaveContext.dayTime},
        {"skybox_time", gSaveContext.skyboxTime}, {"pause_state", play->pauseCtx.state},
        {"cutscene_state", play->csCtx.state}, {"cutscene_frame", play->csCtx.frames},
        {"transition_mode", play->transitionMode}, {"transition_trigger", play->transitionTrigger},
        {"update_rate", R_UPDATE_RATE}, {"health", gSaveContext.health}, {"magic", gSaveContext.magic},
        {"rupees", gSaveContext.rupees}, {"timer_state", gSaveContext.timerState},
        {"timer_seconds", gSaveContext.timerSeconds}, {"subtimer_state", gSaveContext.subTimerState},
        {"subtimer_seconds", gSaveContext.subTimerSeconds}, {"message_mode", play->msgCtx.msgMode},
        {"message_timer", play->msgCtx.stateTimer}, {"text_id", play->msgCtx.textId},
        {"text_draw_pos", play->msgCtx.textDrawPos}, {"ocarina_mode", play->msgCtx.ocarinaMode},
        {"switch_flags", play->actorCtx.flags.swch}, {"temp_switch_flags", play->actorCtx.flags.tempSwch},
        {"chest_flags", play->actorCtx.flags.chest}, {"clear_flags", play->actorCtx.flags.clear}};
    state["global"]["event_flags"] = gSaveContext.eventChkInf;
    state["global"]["event_info"] = gSaveContext.eventInf;
    state["global"]["inventory_items"] = gSaveContext.inventory.items;
    state["global"]["ammo"] = gSaveContext.inventory.ammo;
    auto& in = play->state.input[0];
    state["input"] = {{"held", in.cur.button}, {"pressed", in.press.button}, {"released", in.rel.button},
        {"stick_x", in.cur.stick_x}, {"stick_y", in.cur.stick_y},
        {"right_stick_x", in.cur.right_stick_x}, {"right_stick_y", in.cur.right_stick_y},
        {"previous_buttons", in.prev.button}, {"error", in.cur.err_no}};
    state["controllers"] = json::array();
    for (auto& port : play->state.input) {
        state["controllers"].push_back({{"held", port.cur.button}, {"pressed", port.press.button},
            {"released", port.rel.button}, {"stick_x", port.cur.stick_x}, {"stick_y", port.cur.stick_y},
            {"right_stick_x", port.cur.right_stick_x}, {"right_stick_y", port.cur.right_stick_y},
            {"error", port.cur.err_no}});
    }
    state["player"] = GET_PLAYER(play) ? PlayerState(GET_PLAYER(play)) : json(nullptr);
    if (fixture.value("observe_player_state", false)) state["player_detail"] = PlayerDetail(play);
    state["camera"] = CameraState(play->cameraPtrs[play->activeCamera]);
    state["collision"] = {{"at_count", play->colChkCtx.colATCount},
        {"ac_count", play->colChkCtx.colACCount}, {"oc_count", play->colChkCtx.colOCCount}};
    state["actors"] = json::array();
    for (int category = 0; category < ACTORCAT_MAX; ++category) {
        int order = 0;
        for (Actor* actor = play->actorCtx.actorLists[category].head; actor; actor = actor->next) {
            auto a = ActorState(actor);
            a["list_order"] = order++;
            // Base fields are explicit coverage; per-family action/timer serializers come with fixtures.
            a["coverage"] = "base_actor";
            state["actors"].push_back(std::move(a));
        }
    }
    return state;
}

// These bytes never enter a portable hash or a semantic golden. They detect any
// same-process write, including scratch fields and pointer aliases omitted from
// portable snapshots. No live state/context is swapped or restored.
json playerPoseAdmission = json::object();
std::vector<unsigned char> PresentationLiveBytes(PlayState* play, bool playerPose) {
    std::vector<unsigned char> bytes;
    auto append = [&](const void* data, size_t size) {
        const auto* first = static_cast<const unsigned char*>(data);
        bytes.insert(bytes.end(), first, first + size);
    };
    append(play, sizeof(*play)); // includes View, Font, all inputs, interface and message scratch
    append(&gSaveContext, sizeof(gSaveContext));
    append(gGameInfo, sizeof(*gGameInfo));
    append(gSegments, sizeof(gSegments));
    append(&gPadMgr, sizeof(gPadMgr));
    if (play->interfaceCtx.doActionSegment) append(play->interfaceCtx.doActionSegment, 3 * sizeof(char*));
    NativeSimHudObservation hud{};
    Interface_GetNativeSimHudObservation(play, &hud);
    append(&hud, sizeof(hud));
    NativeSimMessageObservation message{};
    Message_GetNativeSimObservation(play, &message);
    append(&message, sizeof(message));
    unsigned char statics[256]{};
    const auto size = Message_CopyPresentationStatics(statics);
    append(statics, size);
    if (playerPose) {
        const auto temporal = PlayerTemporal_Inspect().dump();
        append(temporal.data(), temporal.size());
        append(GET_PLAYER(play), sizeof(Player));
        for (int category = 0; category < ACTORCAT_MAX; ++category) {
            for (Actor* actor = play->actorCtx.actorLists[category].head; actor; actor = actor->next) {
                append(actor, actor->id == ACTOR_EN_KANBAN ? sizeof(EnKanban) : sizeof(Actor));
            }
        }
        append(&sEffectContext, sizeof(sEffectContext));
        append(&sEffectSsInfo, sizeof(sEffectSsInfo));
        if (sEffectSsInfo.table) append(sEffectSsInfo.table, sEffectSsInfo.tableSize * sizeof(EffectSs));
        append(statics, Player_CopyPoseStatics(statics));
        for (const auto& entry : *GameInteractor::Instance->GetHookData<GameInteractor::OnVanillaBehavior>()) {
            append(&entry.first, sizeof(entry.first));
            append(&entry.second.calls, sizeof(entry.second.calls));
        }
    }
    return bytes;
}
void WritePurity(const char* status, const std::string& failure = "") {
    json result = {{"schema", 1}, {"status", status}, {"fixture", fixture},
        {"extra_calls", 2}, {"packet_bytes_checked", true},
        {"coverage", purityCoverage}, {"admission_negatives", admissionCoverage},
        {"player_admission", playerPoseAdmission},
        {"first_failure", failure}, {"tick", tick}, {"phase", phase},
        {"negative_control", purityNegativeControl},
        {"comparison", "same-process live bytes plus complete semantic state and event sequence; ordered Gfx words and paint"}};
    std::ofstream file(output / "purity.json");
    file << result.dump(2) << '\n';
    if (!file) Fail("purity receipt write failed");
}
void CheckAdmissionNegatives(const std::string& name, PlayState* play) {
    if (admissionCoverage.contains(name)) return;
    auto copy = std::make_unique<PlayState>();
    if (name == "player") {
        Player player;
        Actor poseProbe{};
        ColliderCylinder contactProbe{};
        int tests = 0;
        auto check = [&](const char* label, auto change) {
            std::memcpy(copy.get(), play, sizeof(*play));
            std::memcpy(&player, GET_PLAYER(play), sizeof(player));
            copy->actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
            player.skelAnime.jointTable = reinterpret_cast<Vec3s*>(ALIGN16(reinterpret_cast<uintptr_t>(player.jointTable)));
            std::memcpy(player.skelAnime.jointTable, GET_PLAYER(play)->skelAnime.jointTable,
                        player.skelAnime.limbCount * sizeof(Vec3s) + sizeof(s16));
            Collider* oldColliders[] = { &GET_PLAYER(play)->cylinder.base, &GET_PLAYER(play)->meleeWeaponQuads[0].base,
                                        &GET_PLAYER(play)->meleeWeaponQuads[1].base, &GET_PLAYER(play)->shieldQuad.base };
            Collider* newColliders[] = { &player.cylinder.base, &player.meleeWeaponQuads[0].base,
                                        &player.meleeWeaponQuads[1].base, &player.shieldQuad.base };
            for (int n = 0; n < 4; ++n) {
                newColliders[n]->actor = &player.actor;
                for (int i = 0; i < copy->colChkCtx.colATCount; ++i)
                    if (copy->colChkCtx.colAT[i] == oldColliders[n]) copy->colChkCtx.colAT[i] = newColliders[n];
                for (int i = 0; i < copy->colChkCtx.colACCount; ++i)
                    if (copy->colChkCtx.colAC[i] == oldColliders[n]) copy->colChkCtx.colAC[i] = newColliders[n];
            }
            if (!Player_IsPoseProfileAdmitted(copy.get(), &player))
                Fail("Player negative-admission baseline copy is not admitted");
            change(*copy, player);
            if (Player_IsPoseProfileAdmitted(copy.get(), &player)) {
                WritePurity("fail", std::string("player admission: ") + label);
                Fail("unsupported Player pose admitted");
            }
            ++tests;
        };
        check("pause", [](auto& p, auto&) { p.pauseCtx.state = 1; });
        check("transition", [](auto& p, auto&) { p.transitionTrigger = TRANS_TRIGGER_START; });
        check("reflection", [](auto&, auto& p) { p.stateFlags2 |= PLAYER_STATE2_REFLECTION; });
        check("first person", [](auto&, auto& p) { p.unk_6AD = 2; });
        check("crawl", [](auto&, auto& p) { p.stateFlags2 |= PLAYER_STATE2_CRAWLING; });
        check("carry", [](auto&, auto& p) { p.heldActor = &p.actor; });
        check("get item", [](auto&, auto& p) { p.stateFlags1 |= PLAYER_STATE1_GETTING_ITEM; });
        check("weapon", [](auto&, auto& p) { p.heldItemAction = PLAYER_IA_HAMMER; });
        check("shield", [](auto&, auto& p) { p.currentShield = PLAYER_SHIELD_HYLIAN; });
        check("dynamic floor", [](auto&, auto& p) { p.actor.floorBgId = 0; });
        check("airborne", [](auto&, auto& p) { p.actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND; });
        check("frozen", [](auto&, auto& p) { p.stateFlags2 |= PLAYER_STATE2_FROZEN; });
        check("draw disabled", [](auto&, auto& p) { p.stateFlags2 |= PLAYER_STATE2_DISABLE_DRAW; });
        check("owned child", [](auto&, auto& p) { p.actor.child = &p.actor; });
        check("hostile scene", [](auto& p, auto& player) { p.actorCtx.actorLists[ACTORCAT_ENEMY].head = &player.actor; });
        check("projectile", [&](auto& p, auto&) {
            poseProbe.id = ACTOR_EN_ARROW;
            p.actorCtx.actorLists[ACTORCAT_ITEMACTION].head = &poseProbe;
        });
        auto setContactProbe = [&](auto& p, auto& player, bool attack) {
            contactProbe.base.actor = &poseProbe;
            contactProbe.base.shape = COLSHAPE_CYLINDER;
            contactProbe.base.atFlags = AT_ON | AT_TYPE_ENEMY;
            contactProbe.base.acFlags = AC_ON | AC_TYPE_PLAYER;
            contactProbe.dim.pos = { (s16)player.actor.world.pos.x, (s16)player.actor.world.pos.y,
                                     (s16)player.actor.world.pos.z };
            contactProbe.dim.radius = contactProbe.dim.height = 1;
            if (attack) { p.colChkCtx.colATCount = 1; p.colChkCtx.colAT[0] = &contactProbe.base; }
            else { p.colChkCtx.colACCount = 1; p.colChkCtx.colAC[0] = &contactProbe.base; }
        };
        check("incoming foreign attack", [&](auto& p, auto& player) { setContactProbe(p, player, true); });
        check("unadmitted contact target", [&](auto& p, auto& player) { setContactProbe(p, player, false); });
        check("unknown contact geometry", [&](auto& p, auto& player) {
            setContactProbe(p, player, true);
            contactProbe.base.shape = COLSHAPE_INVALID; // predicate only; never dispatched to collision
        });
        // Test registry entry is never executed; this proves unknown limb hooks
        // reject before callbacks, without loading an external modification.
        using Hooks = GameInteractor::RegisteredGameHooks<GameInteractor::OnVanillaBehavior>;
        auto& limbHooks = Hooks::functionsForID[VB_PLAYER_OVERRIDE_LIMB_DRAW];
        const uint32_t probeId = UINT32_MAX;
        if (limbHooks.count(probeId)) Fail("admission probe ID already registered");
        limbHooks.emplace(probeId, [](GIVanillaBehavior, bool*, va_list) {});
        const bool rejected = !Player_IsPoseProfileAdmitted(play, GET_PLAYER(play));
        limbHooks.erase(probeId);
        if (!rejected) Fail("custom limb hook admitted");
        admissionCoverage[name] = tests + 1;
        return;
    }
    int tests = 0;
    auto check = [&](const char* label, auto change) {
        std::memcpy(copy.get(), play, sizeof(*play));
        change(*copy);
        int admitted = name == "countdown" ? Interface_IsCountdownProfileAdmitted(copy.get()) :
                                             Message_IsPlainTextProfileAdmitted(copy.get());
        if (admitted) { WritePurity("fail", name + " admission: " + label); Fail("unsupported presentation admitted"); }
        ++tests;
    };
    check("pause", [](auto& p) { p.pauseCtx.state = 1; });
    check("freeze", [](auto& p) { p.actorCtx.freezeFlashTimer = 2; });
    check("frame advance", [](auto& p) { p.frameAdvCtx.enabled = 1; });
    check("scene", [](auto& p) { p.sceneNum = SCENE_BOMBCHU_BOWLING_ALLEY; });
    check("transition", [](auto& p) { p.transitionTrigger = TRANS_TRIGGER_START; });
    if (name == "message") {
        check("fade fixture", [](auto& p) { p.msgCtx.textId = 0x305F; });
        check("talker", [](auto& p) { p.msgCtx.talkActor = GET_PLAYER(&p) ? &GET_PLAYER(&p)->actor : (Actor*)&p; });
        check("choice", [](auto& p) { p.msgCtx.choiceNum = 2; });
        check("type", [](auto& p) { p.msgCtx.textBoxType = TEXTBOX_TYPE_OCARINA; });
        check("quicktext control", [](auto& p) { p.msgCtx.font.msgBuf[0] = 0x08; });
        check("length", [](auto& p) { p.msgCtx.msgLength = 65; });
        check("end type", [](auto& p) { p.msgCtx.textboxEndType = TEXTBOX_ENDTYPE_FADING; });
        check("mode", [](auto& p) { p.msgCtx.msgMode = MSGMODE_TEXT_CONTINUING; });
    } else {
        check("message gate", [](auto& p) { p.msgCtx.msgMode = MSGMODE_TEXT_DISPLAYING; });
        check("shooting gallery", [](auto& p) { p.shootingGalleryStatus = 1; });
    }
    admissionCoverage[name] = tests;
}
void WriteSnapshot() {
    if (performanceOnly) return; // Separate performance receipt; never a semantic pass.
    if (AuditCanonicalContacts()) phaseContactBoundaries.push_back(
        {{"tick",tick},{"events",phaseContactEvents},{"order",std::to_string(phaseContactOrder)}});
    auto state = State(gPlayState);
    boundaryRing.Push(state);
    try {
        onlineAssertions->Add(state);
        const auto hash=NativeValidation::Hash(state);
        checkpoints.push_back({{"tick",tick},{"time_q",tick*6},{"sha256",hash}});
        if (!expectedCheckpoints.is_null() && (tick>=expectedCheckpoints.size() ||
            expectedCheckpoints.at(tick).at("sha256")!=hash))
            Fail("common-boundary semantic hash mismatch at tick " + std::to_string(tick));
    } catch (const std::exception& error) { Fail(error.what()); }
    if (detailedSnapshots) {
        snapshots << state.dump() << '\n';
        if (!snapshots) Fail("snapshot write failed");
    }
    if (observeTemporal) {
        auto observation = PlayerTemporal_Inspect();
        if (!observation.at("okay").get<bool>()) Fail("temporal lifecycle/clock contract failed");
        observation["tick"] = tick;
        observation["fixture_time_q"] = tick * PlayerTemporal::WorldStepQuanta;
        auto player = PlayerState(GET_PLAYER(gPlayState));
        observation["player"] = {{"action",player["action"]},{"animation",player["animation"]},
            {"sword_history",player["weapon_geometry"]},{"bg_flags",player["bg_flags"]},
            {"floor_bg_id",player["floor_bg_id"]},{"wall_bg_id",player["wall_bg_id"]}};
        observation["camera"] = CameraState(GET_ACTIVE_CAM(gPlayState));
        temporalSnapshots << observation.dump() << '\n';
        if (!temporalSnapshots) Fail("temporal diagnostic write failed");
    }
}
void ApplyInitialPlayer() {
    if (fixture.value("test_motion_effect",std::string()) == "scoop") {
        // Bottles scoop while wading, not swimming. Find a real shoreline at18 units depth.
        auto* play=gPlayState; auto* p=GET_PLAYER(play); auto* h=play->colCtx.colHeader;
        for(int w=0;w<h->numWaterBoxes;++w) {
            const float level=h->waterBoxes[w].ySurface, floor=level-18;
            for(int i=0;i<h->numPolygons;++i) {
                auto* poly=&h->polyList[i]; if(COLPOLY_GET_NORMAL(poly->normal.y)<0.5f) continue;
                Vec3f v[3],cuts[3];int n=0;
                CollisionPoly_GetVerticesByBgId(poly,BGCHECK_SCENE,&play->colCtx,v);
                for(int j=0;j<3;++j) {
                    const auto& a=v[j];const auto& b=v[(j+1)%3];
                    if((a.y<=floor && b.y>floor)||(b.y<=floor && a.y>floor)) {
                        const float t=(floor-a.y)/(b.y-a.y);
                        cuts[n++]={a.x+t*(b.x-a.x),floor,a.z+t*(b.z-a.z)};
                    }
                }
                if(n!=2) continue;
                Vec3f pos={(cuts[0].x+cuts[1].x)*0.5f,floor,(cuts[0].z+cuts[1].z)*0.5f};
                WaterBox* water=nullptr;float surface=pos.y;
                if(!WaterBox_GetSurface1(play,&play->colCtx,pos.x,pos.z,&surface,&water) || surface!=level) continue;
                p->actor.world.pos=p->actor.prevPos=p->actor.home.pos=pos;
                p->yaw=p->actor.shape.rot.y=p->actor.world.rot.y=0;
                Camera_InitPlayerSettings(GET_ACTIVE_CAM(play),p);
                return;
            }
        }
        Fail("scoop fixture requires a real shallow shoreline");
    }
    if (fixture.value("test_action_wait", std::string()) == "bean") {
        auto* bean = FindBean(gPlayState);
        auto* p = GET_PLAYER(gPlayState);
        p->actor.world.pos = bean->world.pos;
        p->actor.world.pos.z -= 20.0f;
        p->actor.prevPos = p->actor.home.pos = p->actor.world.pos;
        p->yaw = p->actor.world.rot.y = p->actor.shape.rot.y = 0;
        Camera_InitPlayerSettings(GET_ACTIVE_CAM(gPlayState), p);
        return;
    }
    if (fixture.contains("initial_waterbox")) {
        const auto index = fixture.at("initial_waterbox").get<int>();
        const auto* header = gPlayState->colCtx.colHeader;
        if (index < 0 || index >= header->numWaterBoxes) Fail("invalid swim waterbox");
        const auto& water = header->waterBoxes[index];
        auto* player = GET_PLAYER(gPlayState);
        player->actor.world.pos = {water.xMin + water.xLength * 0.5f, water.ySurface - 30.0f,
                                   water.zMin + water.zLength * 0.5f};
        player->actor.prevPos = player->actor.home.pos = player->actor.world.pos;
        Camera_InitPlayerSettings(GET_ACTIVE_CAM(gPlayState), player);
    }
    if (!fixture.contains("initial_player")) return;
    auto& init = fixture.at("initial_player");
    Player* player = GET_PLAYER(gPlayState);
    if (init.contains("pos")) {
        auto& pos = init.at("pos");
        player->actor.world.pos = {pos.at(0).get<float>(),pos.at(1).get<float>(),pos.at(2).get<float>()};
        player->actor.prevPos = player->actor.world.pos;
        if (fixture.value("settle_initial_player",false)) player->actor.home.pos = player->actor.world.pos;
    }
    if (init.contains("yaw")) {
        player->yaw = init.at("yaw").get<int16_t>();
        player->actor.world.rot.y = player->actor.shape.rot.y = player->yaw;
    }
    if (fixture.value("align_camera", false)) Camera_InitPlayerSettings(GET_ACTIVE_CAM(gPlayState), player);
}
void ApplySetup() {
    SetupDynaChecks();
    SetupActionClocks();
    SetupMotionEffects();
    spellWaitChecks = {};
    spellWaitChecks.kind = fixture.value("test_spell_wait", std::string());
    spellWaitChecks.initialMagic = gSaveContext.magic;
    actionRateChecks = {};
    actionRateChecks.kind = fixture.value("test_action_rate", std::string());
    actionRateChecks.analogStruggle = fixture.value("test_analog_struggle", 0) != 0;
    actionWaitChecks = {};
    actionWaitChecks.kind = fixture.value("test_action_wait", std::string());
    actionWaitChecks.initialHealth = gSaveContext.health;
    if (actionWaitChecks.kind == "bean") {
        actionWaitChecks.bean = FindBean(gPlayState);
        if (Flags_GetSwitch(gPlayState, actionWaitChecks.bean->params & 0x3f)) Fail("soil is already planted");
        INV_CONTENT(ITEM_BEAN) = ITEM_BEAN;
        Inventory_ChangeAmmo(ITEM_BEAN, 2 - AMMO(ITEM_BEAN));
        gSaveContext.equips.buttonItems[1] = ITEM_BEAN;
        gSaveContext.equips.cButtonSlots[0] = SLOT_BEAN;
        actionWaitChecks.initialAmmo = AMMO(ITEM_BEAN);
    }
    playerItemTimerChecks.kind = fixture.value("test_item_timer", std::string());
    if (playerItemTimerChecks.kind == "hover") {
        Inventory_ChangeEquipment(EQUIP_TYPE_BOOTS, EQUIP_VALUE_BOOTS_HOVER);
        Player_SetEquipmentData(gPlayState, GET_PLAYER(gPlayState));
    } else if (playerItemTimerChecks.kind == "stick") {
        Inventory_ChangeAmmo(ITEM_STICK, 2 - AMMO(ITEM_STICK));
        gSaveContext.equips.buttonItems[1] = ITEM_STICK;
    }
    if (fixture.contains("setup_aim_item")) {
        const auto item = fixture.at("setup_aim_item").get<uint8_t>();
        gSaveContext.equips.buttonItems[1] = item;
    }
    if (fixture.value("surface_probe", false)) {
        json surfaces = json::array();
        auto* header = gPlayState->colCtx.colHeader;
        for (int i = 0; i < header->numPolygons && surfaces.size() < 128; ++i) {
            auto* poly = &header->polyList[i];
            auto flags = SurfaceType_GetWallFlags(&gPlayState->colCtx, poly, BGCHECK_SCENE);
            if (!(flags & (WALL_FLAG_CLIMBABLE | WALL_FLAG_LADDER | WALL_FLAG_CRAWLSPACE))) continue;
            Vec3f vertices[3];
            CollisionPoly_GetVerticesByBgId(poly, BGCHECK_SCENE, &gPlayState->colCtx, vertices);
            surfaces.push_back({{"index",i},{"flags",flags},{"vertices",{Vec(vertices[0]),Vec(vertices[1]),Vec(vertices[2])}},
                {"normal",{COLPOLY_GET_NORMAL(poly->normal.x),COLPOLY_GET_NORMAL(poly->normal.y),COLPOLY_GET_NORMAL(poly->normal.z)}}});
        }
        std::ofstream file(output / "traversal-surfaces.json"); file << surfaces.dump(2) << '\n';
    }
    if (fixture.value("observe_roll",false)) Player_SetupRoll(GET_PLAYER(gPlayState),gPlayState);
    if (fixture.value("stalfos_arena",0)) {
        for (auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)
            if(a->id==ACTOR_EN_TEST && a->update &&
               (a->params==STALFOS_TYPE_1 || StalfosTemporal_GroundArenaActor(a))) {
                if(stalfosTestActor) Fail("arena spawned more than one duel opponent");
                stalfosTestActor=reinterpret_cast<EnTest*>(a);
            }
        const bool outdoor=fixture.value("stalfos_arena",0)==2;
        if(!stalfosTestActor || (outdoor ? gPlayState->sceneNum!=SCENE_HYRULE_FIELD : gPlayState->roomCtx.curRoom.num!=6))
            Fail("Stalfos arena unavailable");
        if (fixture.value("stalfos_force_culled",false)) {
            // Exercise the real culling decision without changing camera/input
            // or bypassing the selected actor's movement/contact scheduler.
            stalfosTestActor->actor.uncullZoneForward=-100000.0f;
            stalfosTestActor->actor.uncullZoneScale=0;
            stalfosTestActor->actor.uncullZoneDownward=0;
        }
        // A real slash entry gives an exact animation/window timing oracle before
        // ordinary decisions resume. Keep the initial attack out of reach.
        if (!fixture.value("stalfos_contact_mode",std::string()).empty()) {
            auto* p=GET_PLAYER(gPlayState);
            auto& a=stalfosTestActor->actor;
            a.world.pos=p->actor.world.pos; a.world.pos.z-=70.0f;
            a.prevPos=a.world.pos; a.home.pos.y=a.world.pos.y;
            a.shape.rot.y=a.world.rot.y=0;
            a.speedXZ=0; a.velocity={};
            p->actor.shape.rot.y=p->actor.world.rot.y=p->yaw=-0x8000;
            Camera_InitPlayerSettings(GET_ACTIVE_CAM(gPlayState),p);
        }
        EnTest_SetupSlashDown(stalfosTestActor);
        stalfosInitialHealth=stalfosTestActor->actor.colChkInfo.health;
        stalfosPlayerHealth=gSaveContext.health;
    }
    mechanicHealth = gSaveContext.health;
    if (fixture.value("mechanic_case", std::string()) == "bomb") {
        const auto pos = GET_PLAYER(gPlayState)->actor.world.pos;
        auto* bomb = reinterpret_cast<EnBom*>(Actor_Spawn(&gPlayState->actorCtx, gPlayState,
            ACTOR_EN_BOM, pos.x, pos.y, pos.z + 15.0f, 0, 0, 0, BOMB_BODY));
        if (!bomb) Fail("bomb fixture could not spawn bomb");
        bomb->timer = 30; // Shorten only the test's wait; use the real fuse/explosion/collision path.
    }
    if (fixture.value("validate_world_contacts", false)) {
        const auto result = PlayerContact::ValidateWorldContacts();
        { std::ofstream file(output / "world-contact-checks.json"); file << result.dump(2) << '\n'; }
        if (result.at("failures") != 0) Fail("native world contact checks failed");
    }
    if (fixture.contains("setup_shield")) {
        Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, fixture.at("setup_shield").get<uint16_t>());
        Player_SetEquipmentData(gPlayState, GET_PLAYER(gPlayState));
    }
    if (fixture.contains("setup_navi_timer")) gSaveContext.naviTimer = fixture.at("setup_navi_timer").get<uint16_t>();
    if (fixture.contains("message_text_id")) {
        Message_StartTextbox(gPlayState, fixture.at("message_text_id").get<uint16_t>(), nullptr);
    }
    if (fixture.value("spawn_ice_keese", false)) {
        // Fixture construction through the real spawn path, with its scene object
        // already loaded. The actor's unchanged limb draw consumes gameplay RNG.
        int objectIndex = Object_GetIndex(&gPlayState->objectCtx, OBJECT_FIREFLY);
        if (objectIndex < 0 || !Object_IsLoaded(&gPlayState->objectCtx, objectIndex))
            Fail("ice Keese fixture requires the loaded Firefly object bank");
        Camera* camera = GET_ACTIVE_CAM(gPlayState);
        if (!camera || !Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_FIREFLY,
                camera->at.x, camera->at.y + 30.0f, camera->at.z, 0, 0, 0, 4))
            Fail("ice Keese fixture spawn failed");
    }
    if (fixture.contains("hud_timer_seconds")) {
        // Use the real HUD state-machine entry. Interface_Draw owns subsequent
        // preview, movement and per-second countdown; the runner never ticks it.
        Interface_SetTimer(fixture.at("hud_timer_seconds").get<int16_t>());
    }
    if (fixture.contains("ocarina_memory_round")) {
        // This is the real three-note memory-game initialization, including its
        // audio RNG coupling and nonrepeat rule, not synthetic random draws.
        AudioOcarina_MemoryGameInit(fixture.at("ocarina_memory_round").get<uint8_t>());
    }
    if (!fixture.value("settle_initial_player",false)) ApplyInitialPlayer();
    interactionKind = fixture.value("world_interaction", std::string());
    if (!interactionKind.empty()) {
        auto* p = GET_PLAYER(gPlayState);
        s16 id = ACTOR_EN_KUSA, params = 0;
        float distance = 40.0f;
        if (interactionKind == "body" && fixture.value("blocking_mido", false)) {
            Flags_UnsetEventChkInf(EVENTCHKINF_SHOWED_MIDO_SWORD_SHIELD);
            Flags_UnsetEventChkInf(EVENTCHKINF_SPOKE_TO_MIDO_AFTER_DEKU_TREES_DEATH);
            Flags_UnsetEventChkInf(EVENTCHKINF_OBTAINED_ZELDAS_LETTER);
            for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head; a; a = a->next)
                if (a->id == ACTOR_EN_MD && a->update) { interactionTarget = a; break; }
            if (!interactionTarget) Fail("body fixture requires the scene's blocking Mido");
        } else if (interactionKind == "npc" || interactionKind == "body") {
            id = ACTOR_EN_KO; params = -1; distance = 45.0f;
            for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head; a; a = a->next)
                if (a->id == ACTOR_EN_KO && a->update && (a->params & 0xff) < 3) {
                    params = 0xff00 | (a->params & 0xff); break;
                }
            if (params == -1) Fail("NPC interaction needs an existing ordinary Kokiri model in this scene");
        } else if (interactionKind != "grass") Fail("unknown world interaction fixture");
        if (!interactionTarget) interactionTarget = Actor_Spawn(&gPlayState->actorCtx, gPlayState, id,
            p->actor.world.pos.x + Math_SinS(p->yaw) * distance, p->actor.world.pos.y,
            p->actor.world.pos.z + Math_CosS(p->yaw) * distance, 0, p->yaw + 0x8000, 0, params);
        if (!interactionTarget || !interactionTarget->update) Fail("world interaction target could not spawn");
        interactionBushes = gSaveContext.ship.stats.count[COUNT_BUSHES_CUT];
        interactionContacts = PlayerContact::Inspect().at("world_contacts").get<uint64_t>();
    }
    if (fixture.value("spawn_cuttable_sign", false) || fixture.value("spawn_distant_target", false)) {
        int objectIndex = Object_GetIndex(&gPlayState->objectCtx, OBJECT_KANBAN);
        if (objectIndex < 0 || !Object_IsLoaded(&gPlayState->objectCtx, objectIndex))
            Fail("cuttable sign fixture requires the loaded Kanban object bank");
        Player* p = GET_PLAYER(gPlayState);
        const bool distant = fixture.value("spawn_distant_target", false);
        const float distance = distant ? 450.0f : 45.0f;
        Actor* sign = Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_KANBAN,
                p->actor.world.pos.x + Math_SinS(p->yaw) * distance, p->actor.world.pos.y,
                p->actor.world.pos.z + Math_CosS(p->yaw) * distance, 0, p->yaw + 0x8000, 0, 0);
        if (!sign) Fail("cuttable sign fixture spawn failed");
        // Construct a no-contact friendly target with the engine's 700-unit
        // attention range. Default signs only acquire within 70 units, inside
        // the deliberately conservative unbridged-contact exclusion region.
        // No target lock, camera state, actor update or damage is synthesized.
        if (distant) { sign->targetMode = 4; distantTarget = sign; }
        if (fixture.value("validate_contact_bridge",false)) bridgeTarget = sign;
    }
    SetupDoorLockChecks();
}
} // namespace

const json& NativeSimTest_GetFixture() { return fixture; }

extern "C" void NativeSimTest_PlayerStepCommitted(PlayState* play) {
    if (!enabled || !measuring || NativeSimTest_ConfigInt("player_hz",20) == 20) return;
    const auto t=PlayerTemporal_Observe();
    if (fixture.value("bridge_invalidate_target",false) && !bridgeInvalidated &&
        PlayerContact::Inspect().at("pending").get<unsigned>()) {
        if (!bridgeTarget) Fail("contact invalidation requires the controlled live target");
        // Normal actor-lifetime operation, never a fault or stale-pointer dereference.
        Actor_Kill(bridgeTarget); bridgeTarget = nullptr; bridgeInvalidated = true;
    }
    if (!t.okay) Fail("Player scheduler contract failure: " + PlayerTemporal_Inspect().dump());
    for (const char* owner : {"actors", "collision", "blink", "scripts", "environment", "hud", "message", "audio"})
        if (worldOpportunities[owner] != 1) Fail(std::string("world opportunity missing or multiplied: ") + owner);
    Player* player=GET_PLAYER(play);
    ObserveLiveIndicators(player);
    if (fixture.value("observe_live_indicators",false) &&
        (worldOpportunities["player_world_indicators"] != 1 || worldOpportunities["idle_choice"] > 1))
        Fail("world hint/idle producer opportunity multiplied");
    Actor* target=player->focusActor;
    StepObservation current;
    current.temporal=t;current.player=*player;current.worldTick=tick;
    current.cameraInput=GET_ACTIVE_CAM(play)->inputDir;current.cameraMode=GET_ACTIVE_CAM(play)->mode;
    if (target) { current.targetPosition=target->world.pos;current.targetFocus=target->focus.pos; }
    stepRing.Push(current); // Before validation, so the offending observation is retained.
    const unsigned q=120/NativeSimTest_ConfigInt("player_hz",20);
    ObserveSpin(player, highSteps * q);
    if (fixture.contains("mount_response_q") && highSteps * q <= fixture.at("mount_response_q").get<uint64_t>()) {
        const bool expectedMounted = highSteps * q == fixture.at("mount_response_q").get<uint64_t>();
        const bool mounted = (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) && player->actor.parent;
        if (mounted != expectedMounted) Fail("horse mount did not start on the pressed Player step");
    }
    if (interactionKind == "npc" && interactionTalkQ == UINT64_MAX &&
        player->talkActor == interactionTarget && (player->actor.flags & ACTOR_FLAG_TALK))
        interactionTalkQ = highSteps * q;
    if (interactionKind == "npc" && interactionTextboxQ == UINT64_MAX &&
        play->msgCtx.talkActor == interactionTarget && play->msgCtx.msgMode != MSGMODE_NONE)
        interactionTextboxQ = highSteps * q;
    const bool intermediate=t.start%6!=0;
    Actor* expectedTarget = distantTarget ? distantTarget :
        (fixture.value("bridge_z_target",false) ? bridgeTarget : nullptr);
    if (expectedTarget && t.consumingStep==t.step && (t.pressed&BTN_Z) &&
        (target!=expectedTarget || current.cameraMode!=2 || GET_ACTIVE_CAM(play)->target!=target))
        Fail("friendly target/camera not acquired at next Player boundary");
    if (havePreviousStep) {
        const auto& a=previousStep.player;const auto& b=*player;
        const unsigned poseAdvances = !intermediate || PlayerTemporal_PresentationPacket() ? 1 : 0;
        if (t.pose!=previousStep.temporal.pose+poseAdvances) Fail("missing or repeated authoritative pose");
        // Traversal can explicitly align to a wall or attach to a parent after
        // free integration. In particular, grabbed Dyna walls use func_8083F524
        // even without BGCHECKFLAG_WALL. Dyna fixtures separately check the real
        // actor's world intent/motion and unchanged intermediate ownership.
        const bool traversal = (a.stateFlags2 | b.stateFlags2) &
            (PLAYER_STATE2_CRAWLING | PLAYER_STATE2_GRABBING_DYNAPOLY) ||
            (a.stateFlags1 | b.stateFlags1) & (PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_ON_HORSE);
        if (!traversal && a.actionFunc == b.actionFunc && !a.skelAnime.movementFlags && !b.skelAnime.movementFlags &&
            !((a.actor.bgCheckFlags|b.actor.bgCheckFlags)&8) && intermediate) {
            const float dx=b.actor.velocity.x*(q*.25f),dz=b.actor.velocity.z*(q*.25f);
            const auto contact = PlayerContact::ObserveBodyMotion();
            const float x = contact.evaluated ? contact.before[0] : b.actor.world.pos.x;
            const float z = contact.evaluated ? contact.before[2] : b.actor.world.pos.z;
            if (a.actor.world.pos.x+dx!=x || a.actor.world.pos.z+dz!=z ||
                (contact.evaluated && (contact.after[0]!=b.actor.world.pos.x || contact.after[2]!=b.actor.world.pos.z)))
                Fail("horizontal velocity map mismatch");
            ++motionChecks;if (!Equal(a.actor.world.pos,b.actor.world.pos)) ++movingChecks;
        }
        const auto& x=a.skelAnime;const auto& y=b.skelAnime;
        if (!(b.stateFlags1 & PLAYER_STATE1_ON_HORSE) &&
            previousStep.temporal.animation==t.animation && x.morphWeight==0 && y.morphWeight==0 && (y.mode==0 || y.mode==2)) {
            // C-Up and weapon first person hold the base animation:
            // Player_Action_8084B1D8 never calls LinkAnimation_Update on it.
            // Check that hold exactly instead of imposing the idle/slash advance rule.
            const bool heldFirstPerson = a.actionFunc == b.actionFunc &&
                !std::strcmp(NativeSimTest_PlayerActionName(player), "Player_Action_8084B1D8");
            // Freeze deliberately holds its base pose until struggle releases it;
            // the release opportunity itself still executes the frozen branch.
            const bool heldFrozen = a.actionFunc == b.actionFunc && a.av1.actionVar1 >= 0 &&
                !std::strcmp(NativeSimTest_PlayerActionName(player), "Player_Action_8084FB10");
            // Talk closure holds until its world handoff, rather than advancing
            // the base animation on the intervening Player-only opportunities.
            const bool heldTalkClosing = intermediate && a.actionFunc == b.actionFunc &&
                !std::strcmp(NativeSimTest_PlayerActionName(player), "Player_Action_Talk") &&
                Message_GetState(&play->msgCtx) == TEXT_STATE_CLOSING;
            // Handle-door release changes endFrame only after updating against
            // the old zero endpoint. This step must hold zero, then advance next time.
            const bool heldDoorRelease = !intermediate && a.actionFunc == b.actionFunc &&
                !std::strcmp(NativeSimTest_PlayerActionName(player), "Player_Action_80845EF8") &&
                x.mode == 2 && y.mode == 2 && x.curFrame == 0.0f && x.endFrame == 0.0f &&
                y.endFrame == y.animLength - 1.0f && a.av2.actionVar2 == 0 && b.av2.actionVar2 == 1 &&
                a.doorTimer == 1 && b.doorTimer == 0;
            // Parallel/hostile guard blends authored samples through unk_868;
            // func_808401B0 does not advance skelAnime.curFrame after entry.
            const bool heldGuardBlend = a.actionFunc == b.actionFunc && a.av2.actionVar2 == 0 &&
                !std::strcmp(NativeSimTest_PlayerActionName(player), "Player_Action_80840450");
            // Standing shield recoil advances only upperSkelAnime, including
            // the world damage handoff into it and its return to guard.
            const bool heldShieldRecoil =
                (b.av1.actionVar1 == 0 && !std::strcmp(NativeSimTest_PlayerActionName(player), "Player_Action_808435C4")) ||
                (a.av1.actionVar1 == 0 && !std::strcmp(NativeSimTest_PlayerActionName(const_cast<Player*>(&a)), "Player_Action_808435C4"));
            float expected=(heldFirstPerson || heldFrozen || heldTalkClosing || heldDoorRelease || heldGuardBlend || heldShieldRecoil || (y.mode==2 && x.curFrame==y.endFrame)) ?
                x.curFrame : x.curFrame+y.playSpeed*(q*.25f);
            if (y.mode==2 && (expected-y.endFrame)*y.playSpeed>0) expected=y.endFrame;
            else if (expected<0) expected+=y.animLength;
            else if (expected>=y.animLength) expected-=y.animLength;
            if (expected!=y.curFrame) Fail("authored animation phase mismatch: " +
                json({{"action",NativeSimTest_PlayerActionName(player)},
                      {"previous_action",NativeSimTest_PlayerActionName(const_cast<Player*>(&a))},
                      {"previous_frame",x.curFrame},{"expected",expected},{"actual",y.curFrame},
                      {"av1",b.av1.actionVar1},{"av2",b.av2.actionVar2}}).dump());
            ++animationChecks;
        }
        if (intermediate && std::memcmp(&current.cameraInput,&previousStep.cameraInput,sizeof(Vec3s))) ++cameraChanges;
        if (interactionKind == "body" && highSteps * q >= 120 && highSteps * q < 240) {
            const float dx = b.actor.world.pos.x-a.actor.world.pos.x, dz = b.actor.world.pos.z-a.actor.world.pos.z;
            const float distance = std::sqrt(dx*dx+dz*dz);
            if (distance > mechanicBodyMaxStep) {
                mechanicBodyMaxStep = distance;
                const auto contact = PlayerContact::ObserveBodyMotion();
                mechanicBodyPeak = {{"q",t.start},{"old",Vec(a.actor.world.pos)},{"new",Vec(b.actor.world.pos)},
                    {"before_contact",{contact.before[0],contact.before[1],contact.before[2]}},
                    {"after_contact",{contact.after[0],contact.after[1],contact.after[2]}},
                    {"world_displacement",Vec(b.actor.colChkInfo.displacement)},
                    {"constraints",PlayerContact::Inspect().at("body_constraints")}};
            }
            ++mechanicBodySteps;
        }
        if (target && a.focusActor && previousStep.worldTick==tick) {
            if (a.focusActor!=target) Fail("held target identity changed between world boundaries");
            // Only the explicitly converted enemy advances between world boundaries.
            // Its motion/pose and world-owned decisions have separate Stalfos checks.
            if (!StalfosTemporal_IsActor(target) &&
                (!Equal(current.targetPosition,previousStep.targetPosition) ||
                 !Equal(current.targetFocus,previousStep.targetFocus)))
                Fail("held world target changed between world boundaries");
            ++targetChecks;
        }
        // Input fixture times are relative to the first measured interval.
        if (fixture.contains("aim_response_q") && highSteps * q == fixture.at("aim_response_q").get<uint64_t>()) {
            if (a.actor.focus.rot.x == b.actor.focus.rot.x && a.actor.focus.rot.y == b.actor.focus.rot.y)
                Fail("aim did not respond on the first due Player step");
            void* projectionKey = nullptr;
            void* viewingKey = nullptr;
            MtxF projection, viewing;
            PlayerTemporal_PreparedView(&projectionKey, &viewingKey, projection.mf, viewing.mf);
            if (!viewingKey || !std::memcmp(&current.cameraInput, &previousStep.cameraInput, sizeof(Vec3s))) {
                const char* reason = PlayerCamera_ProfileRejection(play);
                Fail(std::string("aiming camera did not prepare the first due Player view: ") +
                     (reason ? reason : "no profile rejection") + (viewingKey ? "; view present" : "; view absent"));
            }
            ++aimResponseChecks;
        }
        for (const auto& event:fixture.at("input")) {
            const uint64_t n=event.at("time_num").get<uint64_t>()*120,d=event.at("time_den").get<uint64_t>();
            if (n%d || event.at("buttons")!=0 || (event.value("stick_x",0)==0 && event.value("stick_y",0)==0)) continue;
            const uint64_t edge=n/d,due=(edge+q-1)/q*q;
            if (!(edge%6) || !(due%6) || highSteps*q!=due) continue;
            // Escape actions consume fresh stick samples without ordinary yaw/
            // speed changes. Their integer gesture score has its own world clock.
            const bool escapeHold = (a.stateFlags2 | b.stateFlags2) &
                (PLAYER_STATE2_FROZEN | PLAYER_STATE2_GRABBED_BY_ENEMY);
            if (t.sequence!=event.at("sequence").get<uint64_t>() || t.consumingStep!=t.step)
                Fail("control input was not consumed at next intermediate Player boundary");
            if (!escapeHold && a.actionFunc==b.actionFunc && a.yaw==b.yaw && a.linearVelocity==b.linearVelocity)
                Fail("control did not respond at next intermediate Player boundary");
            controlChecks.push_back({{"edge_q",edge},{"response_q",due},{"movement_expected",!escapeHold}});
        }
    }
    previousStep=current;havePreviousStep=true;++highSteps;
    if (observeTemporal) {
    const auto state = PlayerTemporal_Inspect();
    if (!playerStepSnapshots.is_open()) playerStepSnapshots.open(output / "player-steps.jsonl");
    if (!playerStepSnapshots) Fail("cannot open Player step observations");
    json heldTarget = target ? json{{"identity",ActorId(target)},{"type",target->id},
        {"position",Vec(target->world.pos)},{"focus",Vec(target->focus.pos)}} : json(nullptr);
    playerStepSnapshots << json{{"tick",tick},{"temporal",state},{"player",PlayerState(GET_PLAYER(play))},
        {"held_target",heldTarget},
        {"camera",CameraState(GET_ACTIVE_CAM(play))},{"world_gameplay_frames",play->gameplayFrames},
        {"world_opportunities",worldOpportunities}}.dump() << '\n';
    playerStepSnapshots.flush();
    if (!playerStepSnapshots) Fail("Player step observation write failed");
    } else if (t.consumingStep==t.step && (t.pressed || t.released)) {
        if (!playerInputEvents.is_open()) playerInputEvents.open(output / "player-events.jsonl");
        playerInputEvents << json{{"tick",tick},{"player_step_id",t.step},{"input_sequence",t.sequence},
            {"pressed",t.pressed},{"released",t.released},{"action",NativeSimTest_PlayerActionName(player)}}.dump() << '\n';
        if (!playerInputEvents) Fail("Player input event write failed");
    }
    if (singleStepControl && !qaPlayerControl.Commit(t.end % 6 == 0)) Fail("Player QA commit without a grant");
}

extern "C" void NativeSimTest_WorldOpportunity(const char* owner) {
    if (enabled && measuring && NativeSimTest_ConfigInt("player_hz",20) != 20 && ++worldOpportunities[owner] != 1)
        Fail(std::string("duplicate world opportunity: ") + owner);
}

extern "C" void* NativeSimTest_Present(const char* helper, PlayState* play, const void* packet, size_t packetSize,
                                       void* outputBuffer, void* paint, size_t paintSize, int visible,
                                       NativeSimPresentationHelper emit) {
    if (!verifyPresentationPurity) return emit(packet, outputBuffer, paint);
    CheckAdmissionNegatives(helper, play);
    const auto beforeBytes = PresentationLiveBytes(play, std::strcmp(helper, "player") == 0);
    const auto beforeState = State(play);
    const auto beforeDrawState = DrawState(play); // also required when fixture observation is off
    const auto beforeSequence = sequence;
    // Both packet builders initialize their entire stack object. These bytes
    // stay process-local, just like the live-state comparison; no pointer or
    // padding is admitted into a portable hash or canonical reference.
    const std::vector<unsigned char> beforePacket(static_cast<const unsigned char*>(packet),
                                                  static_cast<const unsigned char*>(packet) + packetSize);
    const std::vector<unsigned char> initialPaint(static_cast<unsigned char*>(paint),
                                                 static_cast<unsigned char*>(paint) + paintSize);
    auto fail = [&](const std::string& reason) {
        WritePurity("fail", std::string(helper) + ": " + reason);
        Fail(std::string("presentation purity: ") + helper + ": " + reason);
    };
    auto checkLive = [&]() {
        if (std::memcmp(packet, beforePacket.data(), packetSize)) fail("packet mutated");
        if (PresentationLiveBytes(play, std::strcmp(helper, "player") == 0) != beforeBytes || State(play) != beforeState ||
            DrawState(play) != beforeDrawState || sequence != beforeSequence) fail("live state mutated");
    };
    auto* begin = static_cast<Gfx*>(outputBuffer);
    auto* end = static_cast<Gfx*>(emit(packet, outputBuffer, paint));
    checkLive();
    const auto count = end - begin;
    // Both bounded packet schemas emit fewer than 4096 commands. Extra buffers
    // have their own storage; they are never submitted or registered for replay.
    if (count < 0 || count >= 4096) fail("emission exceeds packet command bound");
    for (int repetition = 0; repetition < 2; ++repetition) {
        std::vector<Gfx> scratch(4096);
        auto scratchPaint = initialPaint;
        auto* scratchEnd = static_cast<Gfx*>(emit(packet, scratch.data(), scratchPaint.data()));
        // Explicit test-only detector control exits gracefully, without rendering
        // or another transaction. It is never enabled by an ordinary fixture.
        if (purityNegativeControl && repetition == 0 &&
            (!fixture.value("observe_player_state", false) || std::strcmp(helper, "player") == 0)) {
            if (std::strcmp(helper, "player") == 0) ++GET_PLAYER(play)->unk_845;
            else ++play->msgCtx.stateTimer;
        }
        checkLive();
        if (scratchEnd - scratch.data() != count || std::memcmp(paint, scratchPaint.data(), paintSize))
            fail("paint or command count changed");
        for (ptrdiff_t i = 0; i < count; ++i)
            if (begin[i].words.w0 != scratch[i].words.w0 || begin[i].words.w1 != scratch[i].words.w1)
                fail("ordered command emission changed at " + std::to_string(i));
    }
    NativeSimRecordPresentationCoverage(purityCoverage, helper, measuring, visible, static_cast<uint64_t>(count));
    return end;
}

extern "C" void NativeSimTest_PlayerPoseAdmission(PlayState* play, const char* rejection) {
    if (!verifyPresentationPurity || !measuring || !NativeSimTest_ObservePlayerState()) return;
    const std::string key = rejection ? rejection : "admitted";
    playerPoseAdmission[key] = playerPoseAdmission.value(key, 0u) + 1;
}

extern "C" int NativeSimTest_IsEnabled() { return enabled; }
void NativeSimTest_RecordInputPoll() {
    if (enabled && measuring && performanceOnly) lastInputPoll = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
void NativeSimTest_RecordPresentation(double seconds) {
    if (!enabled || !performanceOnly) return;
    const auto shaderStats = Fast::gShaderCompilationStats;
    if (enabled && measuring && performanceOnly) {
        frameTiming.Add(seconds);
        const bool compiled = shaderStats.compilations != previousShaderStats.compilations;
        // Exclude only directly observed compiler work and its one recovery
        // interval. Keep every raw interval; never classify arbitrary stalls as
        // compilation. A window spanning either interval is not a steady window.
        steadyFrameTiming.Add(seconds, compiled || previousFrameCompiled);
        previousFrameCompiled = compiled;
        measuredShaderCompilations += shaderStats.compilations - previousShaderStats.compilations;
        measuredShaderCacheHits += shaderStats.cacheHits - previousShaderStats.cacheHits;
        measuredShaderCompileMs += shaderStats.milliseconds - previousShaderStats.milliseconds;
        if (lastInputPoll) {
            const double ms = (seconds - lastInputPoll) * 1000;
            maxInputToPresentMs = std::max(maxInputToPresentMs, ms);
            totalInputToPresentMs += ms;
            ++inputToPresentSamples;
            inputToPresentOverBudget += ms > 1000.0 / 120;
        }
    }
    previousShaderStats = shaderStats;
}
extern "C" void NativeSimTest_Presented(int rendered) {
    if (!enabled || !measuring) return;
    if (performanceOnly) completionTiming.Add(
        std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
    if (fixture.value("player_hz", 20) == 120 && fixture.value("presentation_fps", 20) == 120 &&
        PlayerTemporal_PresentationPacket()) {
        // At equal 120-Hz rates each submission must include its own Player
        // update, with no repeated old pose or skipped intermediate update.
        if (PlayerTemporal_NextPlayerOffset() != renderAttempts % 6 + 1)
            Fail("render submitted before its due Player input/update or skipped a Player pose");
        ++currentPlayerPoseSubmissions;
        const auto* packet = static_cast<const PlayerPosePacket*>(PlayerTemporal_PresentationPacket());
        auto* player = GET_PLAYER(gPlayState);
        if (fixture.value("verify_next_frame_response", false)) {
            for (const auto& event : fixture.at("input")) {
                if (!event.value("check_response", false)) continue;
                const uint64_t n = event.at("time_num").get<uint64_t>() * 120;
                const uint64_t d = event.at("time_den").get<uint64_t>();
                if (n % d || n / d != renderAttempts) continue;
                const auto t = PlayerTemporal_Observe();
                const bool poseChanged = havePresentedPose && (packet->matrixCount != previousPresentedPose.matrixCount ||
                    std::memcmp(packet->matrices, previousPresentedPose.matrices, packet->matrixCount * sizeof(Mtx)) != 0);
                const bool response = (event.at("buttons").get<unsigned>() & BTN_B) ?
                    player->actionFunc != previousPresentedPlayer.actionFunc :
                    (player->linearVelocity != previousPresentedPlayer.linearVelocity ||
                     player->yaw != previousPresentedPlayer.yaw ||
                     !Equal(player->actor.world.pos, previousPresentedPlayer.actor.world.pos));
                json receipt = {{"input_q",n/d},{"submission",renderAttempts},{"sequence",t.sequence},
                    {"consumed_this_step",t.consumingStep == t.step},{"response",response},
                    {"pose_changed",poseChanged},{"rendered",rendered != 0}};
                if (!response) receipt["state"] = {{"before_action",NativeSimTest_PlayerActionName(&previousPresentedPlayer)},
                    {"after_action",NativeSimTest_PlayerActionName(player)},
                    {"before_animation",Animation(previousPresentedPlayer.skelAnime)},
                    {"after_animation",Animation(player->skelAnime)},
                    {"before_speed",previousPresentedPlayer.linearVelocity},{"after_speed",player->linearVelocity},
                    {"bg_flags",player->actor.bgCheckFlags},{"flags1",player->stateFlags1},
                    {"flags2",player->stateFlags2},{"flags3",player->stateFlags3}};
                nextFrameResponses.push_back(receipt);
                { std::ofstream file(output / "next-frame-responses.json"); file << nextFrameResponses.dump(2); }
                if (t.sequence != event.at("sequence").get<uint64_t>() || t.consumingStep != t.step ||
                    !response || !poseChanged || !rendered)
                    Fail("next-frame input response did not reach the first due rendered Player pose");
            }
        }
        previousPresentedPose = *packet;
        previousPresentedPlayer = *player;
        havePresentedPose = true;
    }
    if (!renderAttempts) renderStart = std::chrono::steady_clock::now();
    ++renderAttempts;
    renderedFrames += rendered != 0;
}
extern "C" int NativeSimTest_IsMeasuring() { return enabled && measuring; }
extern "C" int NativeSimTest_ConfigInt(const char* key, int fallback) {
    return enabled ? fixture.value(key, fallback) : fallback;
}
extern "C" uint64_t NativeSimTest_TimeQ() { return tick * 6; }
extern "C" uint64_t NativeSimTest_SetupFrame() { return setupFrames; }
extern "C" uint32_t NativeSimTest_Seed() { return fixture.value("seed", 1u); }
extern "C" int NativeSimTest_ObserveDrawState() {
    return enabled && fixture.value("observe_draw_state", false);
}
extern "C" int NativeSimTest_ObservePlayerState() {
    return enabled && fixture.value("observe_player_state", false);
}
extern "C" void NativeSimTest_PlayerSample(const char* site, PlayState* play) {
    if (std::strcmp(site,"collision.begin") == 0) NativeSimTest_WorldOpportunity("collision");
    PlayerTemporal_Sample(site, play);
    ObservePlayerStatus(site, play);
    ObserveActionWait(site, play);
    ObserveActionRate(site, play);
    ObserveSpellWait(site, play);
    ObserveActionClocks(site, play);
    ObserveMotionEffects(site, play);
    if (enabled && measuring && fixture.value("stalfos_arena",0)) {
        static uint64_t beforeRng=0;
        if (!std::strcmp(site,"stalfos.step.begin")) beforeRng=streams["gameplay"].calls;
        if (!std::strcmp(site,"stalfos.step.end") && beforeRng!=streams["gameplay"].calls)
            Fail("Stalfos intermediate work consumed world RNG");
    }
    if (play) ObserveDyna(site,play,&GET_PLAYER(play)->actor);
    ObservePlayerItemTimer(site, play);
    ObserveDoorLockPlayer(site, play);
    if (enabled && measuring && fixture.value("test_talk_closing", false)) {
        auto* p = GET_PLAYER(play);
        if (!std::strcmp(site, "player_step.begin")) {
            if (!dialogueCloseInjected && tick >= 30) {
                if (!(p->actor.flags & ACTOR_FLAG_TALK) || !play->msgCtx.msgLength)
                    Fail("closing fixture did not enter a conversation");
                // Model the automatic offer retained by a Poe until its next update.
                interactionTarget->flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
                Message_CloseTextbox(play);
                dialogueCloseInjected = true;
            }
            dialogueClosingStep = Message_GetState(&play->msgCtx) == TEXT_STATE_CLOSING &&
                (p->actor.flags & ACTOR_FLAG_TALK);
        } else if (!std::strcmp(site, "player_step.end") && dialogueClosingStep) {
            ++dialogueClosingChecks;
            if (!(p->actor.flags & ACTOR_FLAG_TALK))
                Fail("intermediate step released the closing world talk offer");
        }
    }
    if (measuring && interactionKind == "body" && !std::strcmp(site, "collision.after_oc") &&
        GET_PLAYER(play)->cylinder.base.oc == interactionTarget &&
        (GET_PLAYER(play)->cylinder.base.ocFlags1 & OC1_HIT)) ++interactionOverlaps;
    if (!NativeSimTest_ObservePlayerState() || !play || !GET_PLAYER(play)) return;
    if (measuring && fixture.value("observe_action_transitions", false) &&
        (!std::strcmp(site,"pose.end") || !std::strcmp(site,"player_step.end"))) {
        Player* p = GET_PLAYER(play);
        if (!haveTransitionPlayer || p->actionFunc != previousTransitionPlayer.actionFunc ||
            p->skelAnime.animation != previousTransitionPlayer.skelAnime.animation ||
            p->meleeWeaponAnimation != previousTransitionPlayer.meleeWeaponAnimation) {
            if (!actionTransitions.is_open()) actionTransitions.open(output / "action-transitions.jsonl");
            const auto t = PlayerTemporal_Observe();
            const uint64_t now = t.high ? t.start : tick * 6;
            if (haveTransitionPlayer && p->actionFunc != previousTransitionPlayer.actionFunc &&
                !std::strcmp(NativeSimTest_PlayerActionName(&previousTransitionPlayer), "Player_Action_808502D0") &&
                now - transitionActionStart < fixture.value("minimum_attack_quanta", uint64_t(0))) {
                Fail("melee attack completed before its authored terminal interval");
            }
            if (!haveTransitionPlayer || p->actionFunc != previousTransitionPlayer.actionFunc)
                transitionActionStart = now;
            actionTransitions << json{{"time_q",t.high ? t.start : tick*6},
                {"action",NativeSimTest_PlayerActionName(p)},
                {"animation",Animation(GET_PLAYER(play)->skelAnime)},
                {"previous_frame",previousTransitionPlayer.skelAnime.curFrame},
                {"previous_end",previousTransitionPlayer.skelAnime.endFrame},
                {"melee_animation",p->meleeWeaponAnimation},{"melee_state",p->meleeWeaponState},
                {"flags3",p->stateFlags3},{"velocity",p->linearVelocity}}.dump() << std::endl;
        }
        previousTransitionPlayer = *p;
        haveTransitionPlayer = true;
    }
    if (std::strcmp(site, "pose.end") == 0 || std::strcmp(site,"player_step.end") == 0) ++playerPoseGeneration;
    CheckPose(site,GET_PLAYER(play));
    AuditContactSample(site,GET_PLAYER(play));
    if (!measuring || !verbose) return;
    NativeSimTest_TraceJson({{"kind", "player_sample"}, {"site", site},
        {"player", PlayerState(GET_PLAYER(play))}, {"detail", PlayerDetail(play)},
        {"camera", CameraState(GET_ACTIVE_CAM(play))},
        {"input", {{"pressed", play->state.input[0].press.button}, {"held", play->state.input[0].cur.button}}}});
}
extern "C" void NativeSimTest_PlayerActorSample(const char* site, PlayState* play, Actor* actor) {
    ObserveDyna(site,play,actor);
    if (play && actor && actor == (Actor*)GET_PLAYER(play)) {
        PlayerTemporal_Sample(site, play);
        ObservePlayerStatus(site, play);
        ObserveActionWait(site, play);
        ObserveActionRate(site, play);
        ObserveSpellWait(site, play);
        ObserveActionClocks(site, play);
        ObserveMotionEffects(site, play);
        ObservePlayerItemTimer(site, play);
        ObserveDoorLockPlayer(site, play);
    }
    ObserveDoorLockActor(site, play, actor);
    if (!NativeSimTest_ObservePlayerState() || !actor) return;
    if (measuring && actor == interactionTarget && fixture.value("test_talk_closing", false) &&
        !std::strcmp(site, "actor.update.begin") && Message_GetState(&play->msgCtx) == TEXT_STATE_CLOSING) {
        // The controlled actor withdraws its automatic offer when its world
        // update observes closure, as the Poe's soul-disappear transition does.
        actor->flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
    }
    if (measuring && actor == interactionTarget && !std::strcmp(site, "actor.update.end")) ++interactionUpdates;
    if(measuring && actor==reinterpret_cast<Actor*>(stalfosTestActor) && !std::strcmp(site,"actor.update.end"))
        ++stalfosWorldUpdates;
    if (measuring && actor == bridgeTarget && std::strcmp(site,"actor.update.end") == 0) {
        auto* sign = reinterpret_cast<EnKanban*>(actor);
        ++bridgeTargetUpdates;
        if (sign->partFlags != bridgeParts) {
            ++bridgeCuts;
            if (sign->invincibilityTimer != 6) Fail("sign cut did not set six world-tick cooldown");
        } else if (bridgeCooldown > 0 && sign->invincibilityTimer != bridgeCooldown - 1) {
            Fail("sign cooldown did not advance once at world opportunity");
        }
        bridgeParts = sign->partFlags; bridgeCooldown = sign->invincibilityTimer;
    }
    if (measuring && actor==distantTarget && std::strcmp(site,"actor.update.end")==0) ++targetWorldUpdates;
    if (actor == &GET_PLAYER(play)->actor || actor->id == ACTOR_EN_KANBAN)
        AuditContactSample(site,GET_PLAYER(play));
    if (!verbose) return;
    if (actor == &GET_PLAYER(play)->actor || actor->id == ACTOR_EN_KANBAN) {
        // Avoid dispatching the production temporal seam twice for the Player.
        if (measuring) NativeSimTest_TraceJson({{"kind", "player_sample"}, {"site", site},
            {"player", PlayerState(GET_PLAYER(play))}, {"detail", PlayerDetail(play)},
            {"camera", CameraState(GET_ACTIVE_CAM(play))},
            {"input", {{"pressed", play->state.input[0].press.button}, {"held", play->state.input[0].cur.button}}}});
    }
}
extern "C" void NativeSimTest_PlayerRegistration(PlayState* play, const char* category, const void* collider, int index) {
    if (!NativeSimTest_ObservePlayerState() || !measuring) return;
    const auto& c = *static_cast<const Collider*>(collider);
    if (c.actor != &GET_PLAYER(play)->actor) return;
    if (PlayerTemporal_Observe().high) {
        const auto* p = GET_PLAYER(play);
        if (collider != &p->cylinder.base && collider != &p->shieldQuad.base)
            Fail("high-rate sword bypassed retained-sweep contact path");
        const auto& ctx = play->colChkCtx;
        auto count = [&](Collider* const* list, int size) {
            int total = 0; for (int i = 0; i < size; ++i) total += list[i] == collider; return total;
        };
        if (count(ctx.colAT, ctx.colATCount) > 1 || count(ctx.colAC, ctx.colACCount) > 1 ||
            count(ctx.colOC, ctx.colOCCount) > 1) Fail("Player collider registered repeatedly within one world");
    }
    if (AuditCanonicalContacts()) AuditContactRecord({{"kind","player_registration"},{"category",category},
        {"index",index},{"collider",ColliderState(c,play)}});
    if (!verbose) return;
    NativeSimTest_TraceJson({{"kind", "player_registration"}, {"category", category}, {"index", index},
        {"collider", ColliderState(c, play)}});
}
extern "C" void NativeSimTest_PlayerContact(PlayState* play, const void* attack, const void* defense,
                                            uint32_t damageFlags, float x, float y, float z) {
    if (!NativeSimTest_ObservePlayerState()) return;
    const auto& at = *static_cast<const Collider*>(attack);
    const auto& ac = *static_cast<const Collider*>(defense);
    if (at.actor != &GET_PLAYER(play)->actor && ac.actor != &GET_PLAYER(play)->actor) return;
    ++playerContacts;
    if (measuring && PlayerTemporal_Observe().high &&
        !(StalfosTemporal_IsContact() &&
          (StalfosTemporal_IsActor(at.actor) || StalfosTemporal_IsActor(ac.actor))))
        Fail("high-rate Player entered legacy contact");
    if (AuditCanonicalContacts()) AuditContactRecord({{"kind","player_contact"},{"ordinal",playerContacts},
        {"attack",ColliderState(at,play)},{"defense",ColliderState(ac,play)},
        {"damage_flags",damageFlags},{"position",Vec(Vec3f{x,y,z})}});
    if (!verbose) return;
    NativeSimTest_TraceJson({{"kind", "player_contact"}, {"ordinal", playerContacts},
        {"attack", ColliderState(at, play)}, {"defense", ColliderState(ac, play)},
        {"damage_flags", damageFlags}, {"position", Vec(Vec3f{x, y, z})}});
}
extern "C" uint32_t NativeSimTest_AudioClock() {
    if (performanceOnly) return osGetCount();
    // Test-only counter clock. No host time or device occupancy enters this stream.
    return 0x12345678u + 781250u * static_cast<uint32_t>(audioBlocks) + audioClockCalls++;
}
void NativeSimTest_TraceJson(const json& record) {
    if (!enabled || !verbose) return;
    json event = record;
    event["schema"] = 1;
    event["sequence"] = sequence++;
    event["tick"] = tick;
    event["time_q"] = tick * 6;
    event["engine_frame"] = engineFrames;
    event["measuring"] = measuring;
    event["phase"] = phase;
    event["actor"] = ActorId(currentActor);
    // Logging initializes after argument validation. Retain any early events,
    // but do not expose replay file handles to startup's console logger.
    if (!trace.is_open()) {
        pendingTrace.push_back(event.dump());
        return;
    }
    trace << event.dump() << '\n';
    if (!trace) Fail("trace write failed");
}
extern "C" void NativeSimTest_Event(const char* kind, const char* site, uint32_t value) {
    if (!enabled || performanceOnly) return;
    if (measuring && motionEffectChecks.kind=="scoop" && !std::strcmp(kind,"audio-sfx-request") &&
        value==NA_SE_IT_SCOOP_UP_WATER) ++motionEffectChecks.scoopSounds;
    if (std::strcmp(kind, "rng-seed") == 0) {
        if (std::strcmp(site, "Rand_Seed") == 0) streams["gameplay"].state = value;
        else if (std::strcmp(site, "Rand_Seed_Variable") == 0) streams["variable"].state = value;
    } else if (std::strcmp(site, "ShipUtils::RandInit") == 0) {
        if (std::strcmp(kind, "rng-seed-low") == 0) streams["ship-utils"].state = value;
        else if (std::strcmp(kind, "rng-seed-high") == 0) streams["ship-utils-high"].state = value;
    } else if (std::strcmp(kind, "rng-state-high") == 0 && std::strcmp(site, "ShipUtils::next32") == 0) {
        // Companion state word, not an independently advanced generator.
        streams["ship-utils-high"].state = value;
    }
    static const char* authoritativeKinds[] = {
        "rng-seed", "rng-seed-low", "rng-seed-high", "rng-state-high", "rng-output", "rng-draw",
        "audio_block", "audio-sfx-request", "audio-sfx-queued", "audio-sequence-command", "ocarina-memory-note"
    };
    for (const char* authoritativeKind : authoritativeKinds) {
        if (std::strcmp(kind, authoritativeKind) != 0) continue;
        auto& events = streams["events"];
        ++events.calls;
        events.state = value;
        std::string entry = std::string(kind) + ":" + site + ":" + phase + ":" +
                            ActorId(currentActor).dump() + ":" + std::to_string(value);
        for (unsigned char c : entry) events.order = (events.order ^ c) * 1099511628211ull;
        break;
    }
    if (verbose) NativeSimTest_TraceJson({{"kind", kind}, {"site", site}, {"value", value}});
}
extern "C" void NativeSimTest_Rng(const char* stream, const char* site, uint32_t state) {
    if (!enabled || performanceOnly) return;
    auto& rng = streams[stream];
    ++rng.calls;
    if (phase.rfind("draw", 0) == 0) ++rng.drawCalls;
    rng.state = state;
    // Rolling fingerprint retains draw order even when verbose output is disabled.
    std::string entry = std::string(site) + ":" + phase + ":" + ActorId(currentActor).dump() + ":" + std::to_string(state);
    for (unsigned char c : entry) rng.order = (rng.order ^ c) * 1099511628211ull;
    if (verbose) NativeSimTest_TraceJson({{"kind", "rng"}, {"stream", stream}, {"site", site}, {"ordinal", rng.calls}, {"state", state}});
    NativeSimTest_Event("rng-draw", site, state);
}
extern "C" void NativeSimTest_AudioBlock(int samples) {
    if (!enabled) return;
    ++audioBlocks;
    audioSamples += samples;
    NativeSimTest_Event("audio_block", "AudioMgr_CreateNextAudioBuffer", samples);
}
extern "C" void NativeSimTest_ActorSpawn(Actor* actor) {
    PlayerTemporal_ActorCreated(actor);
    if (!enabled) return;
    actorIds[actor] = ++spawnOrdinal;
    if (verbose) NativeSimTest_TraceJson({{"kind", "spawn"}, {"identity", ActorId(actor)}, {"type", actor->id}});
}
extern "C" void NativeSimTest_ActorDestroy(Actor* actor) {
    PlayerTemporal_ActorDestroyed(actor);
    if (!enabled) return;
    if (actor == reinterpret_cast<Actor*>(stalfosTestActor)) stalfosTestActor=nullptr;
    if (actor == interactionTarget) { interactionTarget = nullptr; interactionDestroyed = true; }
    if (verbose) NativeSimTest_TraceJson({{"kind", "destroy"}, {"identity", ActorId(actor)}, {"type", actor->id}});
}
extern "C" void NativeSimTest_ActorFree(Actor* actor) {
    if (!enabled) return;
    if (verbose) NativeSimTest_TraceJson({{"kind", "free"}, {"identity", ActorId(actor)}});
    actorIds.erase(actor);
}
extern "C" void NativeSimTest_SceneInit() {
    PlayerTemporal_SceneInit();
    if (!enabled) return;
    if (measuring) Fail("scene transitions are outside the schema-1 canonical fixture envelope");
    ++sceneEpoch;
    spawnOrdinal = 0;
    distantTarget = nullptr;
    actorIds.clear();
    NativeSimTest_Phase("scene_init", nullptr);
}
extern "C" void NativeSimTest_ActorScope(Actor* actor) {
    if (!enabled) return;
    if (actor) actorScope.push_back(actor);
    else if (!actorScope.empty()) actorScope.pop_back();
    currentActor = actorScope.empty() ? nullptr : actorScope.back();
}
extern "C" void NativeSimTest_Phase(const char* next, PlayState* play) {
    if (!enabled) return;
    if (std::strcmp(next,"draw.interface.begin") == 0) NativeSimTest_WorldOpportunity("hud");
    if (std::strcmp(next,"draw.message.begin") == 0) NativeSimTest_WorldOpportunity("message");
    if (std::strcmp(next,"audio.begin") == 0) NativeSimTest_WorldOpportunity("audio");
    if (std::strcmp(next, "update_begin") == 0) ++updateCalls;
    if (std::strcmp(next, "draw_begin") == 0) ++drawCalls;
    if (verbose && play && measuring) {
        auto state = State(play);
        if (!previousPhase.is_null()) {
            auto diff = json::diff(previousPhase, state);
            if (!diff.empty()) NativeSimTest_TraceJson({{"kind", "phase_mutations"}, {"until", next}, {"changes", diff}});
        }
        previousPhase = std::move(state);
    }
    phase = next;
    if (!actorScope.empty()) Fail("unbalanced actor trace scope at phase boundary");
    currentActor = nullptr;
    NativeSimTest_Event("phase", next, 0);
}
extern "C" void NativeSimTest_Init(int argc, char** argv) {
    std::string fixturePath;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--native-validation-unit-test") {
            try {
                if (++i==argc) throw std::runtime_error("validation unit test requires input");
                std::ifstream file(argv[i]);json cases;file>>cases;
                json results=json::array();
                for (const auto& test:cases.at("cases")) {
                    NativeValidation::Assertions assertions(test.at("fixture"));json hashes=json::array();
                    for (const auto& row:test.at("rows")) { assertions.Add(row);hashes.push_back(NativeValidation::Hash(row)); }
                    results.push_back({{"hashes",hashes},{"assertions",assertions.Result()}});
                }
                NativeValidation::Sha256 empty,abc;
                for (auto c:std::string("abc")) abc.Byte(c);
                NativeValidation::Ring<int,4> ring;for (int n=0;n<100;++n) ring.Push(n);
                json retained=json::array();ring.Visit([&](int n){retained.push_back(n);});
                json result={{"cases",results},{"sha_empty",empty.Finish()},{"sha_abc",abc.Finish()},{"ring",retained}};
                std::ofstream receipt(std::filesystem::path(argv[i]).parent_path()/"native-result.json");
                receipt<<result.dump()<<'\n';receipt.close();
                if (!receipt) throw std::runtime_error("validation receipt write failed");
                std::exit(0);
            } catch (const std::exception& error) { fprintf(stderr,"%s\n",error.what());std::exit(2); }
        }
        if (arg == "--native-sim-first-person-view-test") {
            const unsigned failures = FrameInterpolation_TestFirstPersonView();
            printf("First-person endpoint: %s; %u failures\n", failures ? "FAIL" : "PASS", failures);
            std::exit(failures ? 1 : 0);
        }
        if (arg == "--native-sim-skin-interpolation-test") {
            unsigned checks = 0;
            const unsigned failures = FrameInterpolation_TestSkin(&checks);
            printf("Skin vertex interpolation: %s; %u checks; %u failures\n", failures ? "FAIL" : "PASS", checks, failures);
            std::exit(failures ? 2 : 0);
        }
        if (arg == "--native-sim-animation-queue-test") {
            // Asset-free checks of the real queue implementation. All actor,
            // skeleton and frame-table storage belongs to this private test.
            auto play = std::make_unique<PlayState>();
            PlayerAnimationQueue queue{}, other{};
            Vec3s start[2] = {{10, 20, 30}, {40, 50, 60}};
            Vec3s target[2] = {{30, 40, 50}, {60, 70, 80}};
            Vec3s result[2]{};
            Vec3s filtered[2]{};
            u8 select[2] = {1, 0};
            unsigned checks = 0, failures = 0;
            auto check = [&](bool pass) { ++checks; if (!pass) ++failures; };
            // Equivalent ZYX Euler keys must not produce a half-turn between
            // identical physical poses. Ordinary keys must retain their branch.
            const Vec3s rotation{1000, 100, 2000};
            const Vec3s equivalent{int16_t(1000 + 0x8000), int16_t(0x8000 - 100),
                                   int16_t(2000 + 0x8000)};
            const auto nearest = PlayerAnimation_NearestEuler(rotation, equivalent);
            check(nearest.x == rotation.x && nearest.y == rotation.y && nearest.z == rotation.z);
            const auto ordinary = PlayerAnimation_NearestEuler(rotation, Vec3s{1100, 120, 2100});
            check(ordinary.x == 1100 && ordinary.y == 120 && ordinary.z == 2100);
            MtxF authoredMatrix, sampledMatrix;
            SkinMatrix_SetRotateZYX(&authoredMatrix, equivalent.x, equivalent.y, equivalent.z);
            SkinMatrix_SetRotateZYX(&sampledMatrix, nearest.x, nearest.y, nearest.z);
            for (unsigned row = 0; row < 4; ++row)
                for (unsigned col = 0; col < 4; ++col)
                    check(std::fabs(authoredMatrix.mf[row][col] - sampledMatrix.mf[row][col]) < 0.001f);
            AnimationContext_Update(play.get(), &play->animationCtx);
            check(!PlayerAnimation_BeginQueue(nullptr, &queue));
            check(!PlayerAnimation_BeginQueue(play.get(), nullptr));
            AnimationContext_SetCopyAll(play.get(), 2, result, start);
            check(!PlayerAnimation_BeginQueue(play.get(), &queue));
            check(play->animationCtx.animationCount == 1 && result[0].x == 0);
            AnimationContext_Update(play.get(), &play->animationCtx);
            check(result[0].x == 10 && result[1].z == 60);
            AnimationContext_DisableQueue(play.get());
            check(PlayerAnimation_BeginQueue(play.get(), &queue));
            check(!PlayerAnimation_BeginQueue(play.get(), &other));
            check(!PlayerAnimation_EndQueue(&other));
            AnimationContext_SetCopyAll(play.get(), 2, result, start);
            AnimationContext_SetInterp(play.get(), 2, result, target, 0.5f);
            AnimationContext_SetNextQueue(play.get());
            AnimationContext_DisableQueue(play.get());
            AnimationContext_SetCopyAll(play.get(), 2, result, target);
            AnimationContext_SetNextQueue(play.get());
            AnimationContext_SetCopyTrue(play.get(), 2, result, target, select);
            AnimationContext_SetCopyFalse(play.get(), 2, filtered, start, select);
            Actor actor{};
            actor.scale = {1, 1, 1};
            Vec3s root = {0, 12, 0};
            SkelAnime animation{};
            animation.jointTable = &root;
            animation.prevTransl.y = 2;
            animation.movementFlags = ANIM_FLAG_UPDATEY;
            AnimationContext_SetMoveActor(play.get(), &actor, &animation, 1.0f);
            check(play->animationCtx.animationCount == 0 && queue.queue.animationCount == 6);
            check(actor.world.pos.y == 0 && result[0].x == 10);
            check(PlayerAnimation_EndQueue(&queue));
            check(result[0].x == 30 && result[0].z == 50 && result[1].x == 50 && result[1].z == 70);
            check(filtered[0].x == 0 && filtered[1].x == 40 && filtered[1].z == 60);
            check(actor.world.pos.y == 10 && root.y == 0 && animation.prevTransl.y == 12);
            check(queue.queue.animationCount == 0 && play->animationCtx.animationCount == 0);
            check(!PlayerAnimation_EndQueue(&queue) && actor.world.pos.y == 10);
            // The previous world disable mask must survive the private drain.
            AnimationContext_SetCopyAll(play.get(), 2, result, start);
            AnimationContext_Update(play.get(), &play->animationCtx);
            check(result[0].x == 30);
            check(PlayerAnimation_BeginQueue(play.get(), &queue));
            for (unsigned n = 0; n <= ANIMATION_ENTRY_MAX; ++n) {
                AnimationContext_SetCopyAll(play.get(), 2, result, start);
            }
            check(queue.queue.animationCount == ANIMATION_ENTRY_MAX && queue.overflowed);
            check(!PlayerAnimation_EndQueue(&queue));
            check(result[0].x == 10 && queue.queue.animationCount == 0);
            check(PlayerAnimation_BeginQueue(play.get(), &queue));
            check(PlayerAnimation_EndQueue(&queue));
            // The real Player input consumer, with private accumulated samples.
            // No controller/window service or retrace callback is initialized.
            Input pads[4]{};
            Input consumed{};
            pads[0].press.button = pads[0].rel.button = BTN_B;
            pads[0].press.stick_x = 12;
            pads[1].press.button = BTN_START;
            const Input queued = pads[0];
            const Input otherPort = pads[1];
            check(PadMgr_ConsumePlayerSample(&pads[0], &consumed, 0));
            check(std::memcmp(&pads[0], &queued, sizeof(Input)) == 0 && consumed.press.button == BTN_B);
            check(PadMgr_ConsumePlayerSample(&pads[0], &consumed, 1));
            check(consumed.cur.button == 0 && consumed.press.button == BTN_B && consumed.rel.button == BTN_B);
            check(pads[0].press.button == 0 && pads[0].rel.button == 0 && pads[0].press.stick_x == 0);
            check(std::memcmp(&pads[1], &otherPort, sizeof(Input)) == 0);
            check(PadMgr_ConsumePlayerSample(&pads[0], &consumed, 1) && consumed.press.button == 0 && consumed.rel.button == 0);
            pads[0].cur.button = BTN_Z;
            check(PadMgr_ConsumePlayerSample(&pads[0], &consumed, 1) && consumed.cur.button == BTN_Z && pads[0].cur.button == BTN_Z);
            pads[0].press.button = BTN_START | BTN_B;
            const Input blocked = pads[0];
            const Input previousOutput = consumed;
            check(!PadMgr_ConsumePlayerSample(&pads[0], &consumed, 1));
            check(std::memcmp(&pads[0], &blocked, sizeof(Input)) == 0 &&
                  std::memcmp(&consumed, &previousOutput, sizeof(Input)) == 0);
            check(!PadMgr_ConsumePlayerSample(&pads[0], &pads[0], 1));
            check(!PadMgr_ConsumePlayerSample(nullptr, &consumed, 1));
            printf("Player animation queue: %s; %u checks; %u failures\n", failures ? "FAIL" : "PASS", checks, failures);
            std::exit(failures ? 2 : 0);
        }
        if (arg == "--native-sim-test") {
            enabled = true;
            if (++i == argc) Fail("--native-sim-test requires a fixture path");
            fixturePath = argv[i];
        } else if (arg == "--output" && i + 1 < argc) output = argv[++i];
        else if (arg == "--trace") verbose = detailedSnapshots = true;
        else if (arg == "--diagnostic-snapshots") detailedSnapshots = true;
        else if (arg == "--reference-checkpoints") {
            if (++i==argc) Fail("--reference-checkpoints requires a path");
            referenceCheckpoints=argv[i];
        }
        else if (arg == "--verify-presentation-purity") verifyPresentationPurity = true;
        else if (arg == "--presentation-purity-negative-control") purityNegativeControl = true;
        else if (arg == "--observe-temporal") observeTemporal = true;
        else if (arg == "--native-sim-step-control") singleStepControl = observeTemporal = true;
    }
    if ((verifyPresentationPurity || purityNegativeControl) && !enabled)
        Fail("presentation purity options require --native-sim-test");
    if (purityNegativeControl && !verifyPresentationPurity)
        Fail("negative control requires --verify-presentation-purity");
    if ((observeTemporal || singleStepControl || detailedSnapshots || !referenceCheckpoints.empty()) && !enabled)
        Fail("temporal QA options require --native-sim-test");
    if (!enabled) return;
    try {
        if (output.empty()) throw std::runtime_error("test mode requires --output");
        std::filesystem::create_directories(output);
        if (std::filesystem::exists(output / "result.json") || std::filesystem::exists(output / "snapshots.jsonl") ||
            std::filesystem::exists(output / "checkpoints.json") || std::filesystem::exists(output / "player-events.jsonl") ||
            std::filesystem::exists(output / "failure-boundaries.jsonl") ||
            std::filesystem::exists(output / "trace.jsonl") || std::filesystem::exists(output / "temporal.jsonl") ||
            std::filesystem::exists(output / "qa-state.json") || std::filesystem::exists(output / "qa-command.json"))
            throw std::runtime_error("test output already contains results; choose a fresh directory");
        std::ifstream file(fixturePath);
        if (!file) throw std::runtime_error("cannot open fixture");
        file >> fixture;
        if (!fixture.is_object()) throw std::runtime_error("fixture must be a JSON object");
        performanceOnly = fixture.value("performance_only", 0) != 0;
        if (performanceOnly && (verbose || detailedSnapshots || observeTemporal || verifyPresentationPurity ||
            singleStepControl || !referenceCheckpoints.empty() || !fixture.at("assertions").empty()))
            throw std::runtime_error("performance-only runs cannot claim semantic/assertion/trace qualification");
        auto integer = [](const json& object, const char* key, int64_t minimum, int64_t maximum,
                          int64_t fallback, bool required = false) {
            auto item = object.find(key);
            if (item == object.end()) {
                if (required) throw std::runtime_error(std::string("fixture missing ") + key);
                return fallback;
            }
            if (!item->is_number_integer() ||
                (item->is_number_unsigned() && item->get<uint64_t>() > static_cast<uint64_t>(INT64_MAX)))
                throw std::runtime_error(std::string("fixture ") + key + " must be an integer");
            int64_t value = item->get<int64_t>();
            if (value < minimum || value > maximum)
                throw std::runtime_error(std::string("fixture ") + key + " is outside its supported range");
            return value;
        };
        integer(fixture, "schema", 1, 1, 1, true);
        integer(fixture, "rate_hz", 20, 20, 20);
        const auto playerHz = integer(fixture,"player_hz",20,120,20);
        if (playerHz != 20 && playerHz != 60 && playerHz != 120) throw std::runtime_error("unsupported Player rate");
        const auto& id = fixture.at("id");
        if (!id.is_string() || id.get<std::string>().empty()) throw std::runtime_error("fixture id must be nonempty text");
        integer(fixture, "ticks", 1, 100000, 0, true);
        const auto setupTicks = integer(fixture, "setup_ticks", 1, 10000, 60);
        integer(fixture, "initial_player_setup_frame", 0, setupTicks - 1, 10);
        integer(fixture, "seed", 0, UINT32_MAX, 1);
        integer(fixture, "age", 0, 1, 1);
        // Play_Init selects one of four day/age entrance-layer entries.
        integer(fixture, "entrance", 0, ENTR_MAX - 4, ENTR_LINKS_HOUSE_CHILD_SPAWN);
        integer(fixture, "presentation_fps", 20, 360, 20);
        integer(fixture, "hud_timer_seconds", 1, 3599, 1);
        integer(fixture, "ocarina_memory_round", 0, 2, 0);
        integer(fixture, "message_text_id", 0, UINT16_MAX, 0);
        integer(fixture, "setup_navi_timer", 0, 25800, 0);
        integer(fixture, "setup_shield", 0, 2, 1);
        integer(fixture, "unrestricted_player", 0, 1, 0);
        integer(fixture, "visited_death_mountain", 0, 1, 0);
        if (integer(fixture, "test_analog_struggle", 0, 1, 0) &&
            fixture.value("test_action_rate", std::string()) != "freeze")
            throw std::runtime_error("analog struggle fixture requires freeze action check");
        if (fixture.contains("test_action_rate")) {
            const auto kind = fixture.at("test_action_rate").get<std::string>();
            if ((kind != "freeze" && kind != "electric" && kind != "hookshot") || performanceOnly ||
                !fixture.value("observe_player_state",false) || (playerHz != 20 && playerHz != 120))
                throw std::runtime_error("action rate fixture requires observed Player20/120 and freeze, electric or hookshot");
        }
        if (fixture.contains("test_action_wait")) {
            const auto kind = fixture.at("test_action_wait").get<std::string>();
            if ((kind != "bean" && kind != "knockback") || performanceOnly ||
                !fixture.value("observe_player_state", false) || (playerHz != 20 && playerHz != 120))
                throw std::runtime_error("action wait fixture requires observed Player20/120 and bean or knockback");
        }
        if (fixture.contains("test_spell_wait")) {
            const auto kind = fixture.at("test_spell_wait").get<std::string>();
            if ((kind != "din" && kind != "farore") || performanceOnly ||
                !fixture.value("observe_player_state",false) || (playerHz != 20 && playerHz != 120))
                throw std::runtime_error("spell wait fixture requires observed Player20/120 and din or farore");
        }
        if (fixture.contains("test_dyna")) {
            const auto kind = fixture.at("test_dyna").get<std::string>();
            if ((kind != "push" && kind != "lift") || performanceOnly ||
                !fixture.value("observe_player_state",false) || (playerHz != 20 && playerHz != 120))
                throw std::runtime_error("dyna fixture requires observed Player20/120 and push or lift");
        }
        if (fixture.contains("test_action_clock")) {
            const auto kind = fixture.at("test_action_clock").get<std::string>();
            if ((kind != "fishing" && kind != "gallery" && kind != "carry" && kind != "heavy" && kind != "lift" && kind != "void") ||
                performanceOnly || !fixture.value("observe_player_state",false) || (playerHz != 20 && playerHz != 120))
                throw std::runtime_error("action clock fixture requires observed Player20/120 and a known action");
        }
        if (fixture.contains("test_motion_effect")) {
            const auto kind=fixture.at("test_motion_effect").get<std::string>();
            if ((kind!="bunny" && kind!="scoop" && kind!="swim" && kind!="slope" && kind!="uphill" && kind!="air") || performanceOnly ||
                !fixture.value("observe_player_state",false) || (playerHz!=20 && playerHz!=120))
                throw std::runtime_error("motion/effect fixture requires observed Player20/120 and a known scenario");
        }
        if (fixture.contains("test_item_timer")) {
            const auto kind = fixture.at("test_item_timer").get<std::string>();
            if ((kind != "hover" && kind != "stick") || performanceOnly ||
                !fixture.value("observe_player_state", false) || (playerHz != 20 && playerHz != 120) ||
                fixture.at("ticks").get<int>() != (kind == "hover" ? 60 : 250))
                throw std::runtime_error("item timer fixture requires observed Player20/120 and60 hover or250 stick ticks");
        }
        for (const char* key : {"observe_player_state", "spawn_cuttable_sign", "spawn_distant_target",
             "validate_contact_bridge", "bridge_invalidate_target", "bridge_require_duplicates", "bridge_z_target",
             "observe_live_indicators", "require_live_indicators", "require_first_person", "test_player_status_cadence",
             "test_door_lock"})
            if (fixture.contains(key) && !fixture.at(key).is_boolean())
                throw std::runtime_error(std::string(key) + " must be a boolean");
        if (fixture.value("test_player_status_cadence", false) &&
            (performanceOnly || !fixture.value("observe_player_state", false) ||
             fixture.at("ticks").get<int>() != 80 || (playerHz != 20 && playerHz != 120)))
            throw std::runtime_error("Player status cadence requires80 observed world ticks at Player20 or120");
        if (fixture.value("test_door_lock", false) &&
            (performanceOnly || !fixture.value("observe_player_state", false) ||
             fixture.at("ticks").get<int>() != 18 || (playerHz != 20 && playerHz != 120) ||
             fixture.value("entrance", -1) != ENTR_FIRE_TEMPLE_ENTRANCE || fixture.value("age", 1) != 0))
            throw std::runtime_error("door lock fixture requires18 observed adult Fire Temple ticks at Player20 or120");
        if (fixture.value("spawn_cuttable_sign", false) && !fixture.value("observe_player_state", false))
            throw std::runtime_error("cuttable sign recipe requires Player observation");
        if (fixture.value("validate_contact_bridge",false)) {
            if (playerHz == 20 || !fixture.value("spawn_cuttable_sign",false) ||
                !fixture.value("observe_player_state",false))
                throw std::runtime_error("contact bridge requires high-rate controlled sign observation");
            integer(fixture,"expected_sign_cut_type",0,5,0,true);
        }
        for (const char* key : {"bridge_invalidate_target", "bridge_require_duplicates", "bridge_z_target"})
            if (fixture.value(key,false) && !fixture.value("validate_contact_bridge",false))
                throw std::runtime_error("contact bridge control requires validate_contact_bridge");
        if (fixture.contains("observe_draw_state") && !fixture.at("observe_draw_state").is_boolean())
            throw std::runtime_error("observe_draw_state must be a boolean");
        if (fixture.contains("spawn_ice_keese") && !fixture.at("spawn_ice_keese").is_boolean())
            throw std::runtime_error("spawn_ice_keese must be a boolean");
        if (fixture.contains("initial_player")) {
            const auto& player = fixture.at("initial_player");
            if (!player.is_object()) throw std::runtime_error("initial_player must be an object");
            integer(player, "yaw", INT16_MIN, INT16_MAX, 0);
            if (player.contains("pos")) {
                const auto& pos = player.at("pos");
                if (!pos.is_array() || pos.size() != 3)
                    throw std::runtime_error("initial_player pos must contain exactly three coordinates");
                for (const auto& coordinate : pos) {
                    if (!coordinate.is_number() || !std::isfinite(coordinate.get<double>()) ||
                        std::abs(coordinate.get<double>()) > 32767.0)
                        throw std::runtime_error("initial_player coordinate is outside the finite scene range");
                }
            }
        }
        NativeSimTest_ValidateInput();
        onlineAssertions=std::make_unique<NativeValidation::Assertions>(fixture);
        if (!referenceCheckpoints.empty()) {
            std::ifstream reference(referenceCheckpoints);json document;reference>>document;
            if (document.at("format")!="semantic-sha256-v1" || document.at("fixture")!=fixture)
                throw std::runtime_error("incompatible reference checkpoints");
            expectedCheckpoints=document.at("snapshots");
            if (expectedCheckpoints.size()!=fixture.at("ticks").get<size_t>()+1)
                throw std::runtime_error("incomplete reference checkpoints");
            for (size_t i=0;i<expectedCheckpoints.size();++i)
                if (expectedCheckpoints[i].at("tick")!=i || expectedCheckpoints[i].at("time_q")!=i*6)
                    throw std::runtime_error("invalid reference checkpoint clock");
        }
    } catch (const std::exception& error) { Fail(error.what()); }
}
extern "C" void NativeSimTest_Configure() {
    if (!enabled) return;
    // Context::InitLogging has replaced the startup logger by this point.
    // Opening before that allowed stale console handles to corrupt JSON output.
    if (observeTemporal) detailedSnapshots=true; // explicit diagnostic/QA request
    if (detailedSnapshots) snapshots.open(output / "snapshots.jsonl");
    if (verbose) trace.open(output / "trace.jsonl");
    if (observeTemporal) temporalSnapshots.open(output / "temporal.jsonl");
    if (observeTemporal && !temporalSnapshots) Fail("cannot open temporal diagnostics");
    if ((detailedSnapshots && !snapshots) || (verbose && !trace)) Fail("cannot open output files");
    for (const auto& event : pendingTrace) trace << event << '\n';
    pendingTrace.clear();
    if (verbose && !trace) Fail("buffered trace write failed");
    CVarSetInteger(CVAR_SETTING("InterpolationFPS"), NativeSimTest_ConfigInt("presentation_fps", 20));
    CVarSetInteger(CVAR_SETTING("MatchRefreshRate"), 0);
    if (!performanceOnly) CVarSetInteger(CVAR_VSYNC_ENABLED, 0);
    CVarSetInteger(CVAR_GENERAL("LetItSnow"), 0);
    CVarSetInteger(CVAR_CHEAT("TimeSync"), 0);
    CVarSetInteger(CVAR_SETTING("EnableMouse"), 0);
    CVarSetInteger(CVAR_SETTING("Mods.AlternateAssetsHotkey"), 0);
    CVarSetInteger(CVAR_CHEAT("SaveStatesEnabled"), 0);
    CVarSetInteger(CVAR_CHEAT("EasyFrameAdvance"), 0);
    CVarSetInteger(CVAR_REMOTE_CROWD_CONTROL("Enabled"), 0);
    CVarSetInteger(CVAR_REMOTE_SAIL("Enabled"), 0);
    CVarSetInteger(CVAR_REMOTE_ANCHOR("Enabled"), 0);
}
extern "C" void NativeSimTest_PumpPausedWindow();
extern "C" void NativeSimTest_WaitPlayer(unsigned offset) {
    if (!enabled || !measuring || !singleStepControl || NativeSimTest_ConfigInt("player_hz",20) == 20) return;
    const auto heldState = State(gPlayState).dump();
    const auto heldTemporal = PlayerTemporal_Inspect().dump();
    bool reported = false;
    for (;;) {
        const auto path = output / "qa-command.json";
        if (std::filesystem::exists(path)) {
            json command;
            { std::ifstream file(path); command = json::parse(file,nullptr,false); }
            if (command.is_object() && command.contains("sequence") && command["sequence"].is_number_unsigned()) {
                const uint64_t seq = command["sequence"].get<uint64_t>();
                if (seq > qaSequence) {
                    if (seq != qaSequence+1 || command.value("tick",UINT64_MAX) != tick ||
                        command.value("player_offset_q",UINT_MAX) != offset)
                        Fail("QA command sequence or Player boundary mismatch");
                    const auto op = command.value("operation",std::string());
                    bool accepted = false;
                    if (op == "step_player") accepted = qaPlayerControl.Step();
                    else if (op == "next_world") accepted = qaPlayerControl.NextWorld(tick+1);
                    else if (op == "run") accepted = qaPlayerControl.Run();
                    else if (op == "pause") accepted = qaPlayerControl.Pause();
                    if (!accepted) Fail("invalid Player QA operation or grant state");
                    qaPlayerCommands.push_back(command);
                    qaSequence = seq; reported = false;
                }
            }
        }
        if (qaPlayerControl.Begin(tick+1)) break;
        if (!reported) {
            ++qaHolds;
            json state = {{"schema",1},{"status","paused"},{"tick",tick},{"player_offset_q",offset},
                {"sequence",qaSequence},{"player_hz",NativeSimTest_ConfigInt("player_hz",20)},
                {"world_gameplay_frames",gPlayState->gameplayFrames},{"temporal",PlayerTemporal_Inspect()},
                {"player",PlayerState(GET_PLAYER(gPlayState))},{"detail",PlayerDetail(gPlayState)},
                {"camera",CameraState(GET_ACTIVE_CAM(gPlayState))}};
            std::ofstream file(output / "qa-state.json"); file << state.dump(2) << '\n';
            if (!file) Fail("cannot write Player QA inspection");
            reported = true;
        }
        NativeSimTest_PumpPausedWindow();
        if (!WindowIsRunning()) Fail("QA window closed before fixture completion");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (State(gPlayState).dump() != heldState || PlayerTemporal_Inspect().dump() != heldTemporal)
        Fail("Player QA hold changed authoritative state or temporal metadata");
}
extern "C" void NativeSimTest_WaitFrame() {
    if (!enabled || !measuring) return;
    if (singleStepControl && NativeSimTest_ConfigInt("player_hz",20) != 20) {
        NativeSimTest_WaitPlayer(0);
        qaControl.Run();
        if (!qaControl.Begin()) Fail("duplicate world QA grant");
        return;
    }
    if (singleStepControl) {
        const auto heldState = State(gPlayState).dump();
        const auto heldTemporal = PlayerTemporal_Inspect().dump();
        bool reported = false;
        for (;;) {
            auto path = output / "qa-command.json";
            if (std::filesystem::exists(path)) {
                json command;
                { std::ifstream file(path); command = json::parse(file,nullptr,false); }
                if (command.is_object() && command.contains("sequence") && command["sequence"].is_number_unsigned()) {
                    const uint64_t seq = command["sequence"].get<uint64_t>();
                    if (seq > qaSequence) {
                        if (seq != qaSequence+1 || !command.contains("tick") || command["tick"] != tick)
                            Fail("QA command sequence or canonical boundary mismatch");
                        const auto op = command.value("operation",std::string());
                        if (op == "step") { if (!qaControl.Step()) Fail("QA step requires a paused boundary"); }
                        else if (op == "run") qaControl.Run();
                        else if (op == "pause") qaControl.Pause();
                        else Fail("unsupported QA operation");
                        qaSequence = seq; reported = false;
                    }
                }
            }
            if (qaControl.Begin()) break;
            if (!reported) {
                ++qaHolds;
                json state = {{"schema",1},{"status","paused"},{"tick",tick},
                    {"sequence",qaSequence},{"canonical_time_q",qaControl.time.quanta},
                    {"canonical_transaction_id",qaControl.transactionId},{"temporal",PlayerTemporal_Inspect()}};
                std::ofstream file(output / "qa-state.json"); file << state.dump(2) << '\n';
                if (!file) Fail("cannot write QA boundary inspection");
                reported = true;
            }
            NativeSimTest_PumpPausedWindow();
            if (!WindowIsRunning()) Fail("QA window closed before fixture completion");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (State(gPlayState).dump() != heldState || PlayerTemporal_Inspect().dump() != heldTemporal)
            Fail("QA hold changed authoritative state or temporal metadata");
    } else if (!qaControl.Begin()) Fail("duplicate canonical transaction grant");
}
extern "C" void NativeSimTest_BeginFrame() {
    const auto beforeStatus = PlayerTemporal_RateStatus();
    PlayerTemporal_BeginFrame();
    if (enabled && measuring && fixture.value("observe_live_indicators",false)) {
        const auto afterStatus = PlayerTemporal_RateStatus();
        if (beforeStatus.effective != afterStatus.effective || beforeStatus.admitted != afterStatus.admitted ||
            beforeStatus.fallbackLatched != afterStatus.fallbackLatched)
            Fail("host frame reset changed persistent Player admission status");
        ++liveStatusFrames;
    }
    worldOpportunities.clear();
    if (!enabled) return;
    ++engineFrames;
    updateCalls = drawCalls = 0;
    NativeSimTest_Phase("input_poll", nullptr);
}
extern "C" void NativeSimTest_EndFrame() {
    PlayerTemporal_EndFrame();
    if (!enabled) return;
    NativeSimTest_Phase("transaction_end", gPlayState);
    if (!gPlayState || !GET_PLAYER(gPlayState)) Fail("fixture did not initialize Player");
    if (!measuring) {
        ++setupFrames;
        // Frame zero requests the earliest boundary with a fully initialized Player.
        const auto placementFrame = std::max(1, NativeSimTest_ConfigInt("initial_player_setup_frame", 10));
        if (fixture.value("settle_initial_player",false) &&
            setupFrames == static_cast<uint64_t>(placementFrame)) ApplyInitialPlayer();
        if (setupFrames >= static_cast<uint64_t>(NativeSimTest_ConfigInt("setup_ticks", 60))) {
            if (R_UPDATE_RATE != 3) Fail("setup did not reach canonical world cadence");
            ApplySetup();
            measuring = true;
            if (singleStepControl) { qaControl.Pause(); qaPlayerControl.Pause(); }
            previousPhase = nullptr;
            WriteSnapshot();
        }
        return;
    }
    if (R_UPDATE_RATE != 3 || updateCalls != 1 || drawCalls != 1)
        Fail("fixture left canonical cadence or did not execute exactly one update and CPU draw");
    ObserveDoorLockBoundary(gPlayState);
    ObserveLiveIndicators(GET_PLAYER(gPlayState));
    if (fixture.contains("mechanic_case")) {
        const auto* p = GET_PLAYER(gPlayState);
        if (NativeSimTest_ConfigInt("player_hz",20) == 20) ObserveSpin(GET_PLAYER(gPlayState), tick * 6);
        if (p->heldActor && p->heldActor->id == ACTOR_EN_BOM) {
            ++mechanicHeldBombSteps;
            mechanicHeldExplosionSteps += p->heldActor->params == BOMB_EXPLOSION;
        }
        mechanicDamageSteps += (p->stateFlags1 & PLAYER_STATE1_DAMAGED) != 0;
        mechanicWaterTicks += (p->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0;
        mechanicCurrentTicks += (p->stateFlags1 & PLAYER_STATE1_IN_WATER) && p->pushedSpeed != 0;
        mechanicCrawlSteps += (p->stateFlags2 & PLAYER_STATE2_CRAWLING) != 0;
        mechanicCrawlCameraSteps += GET_ACTIVE_CAM(gPlayState)->setting == CAM_SET_CRAWLSPACE;
        if ((p->stateFlags1 & PLAYER_STATE1_ON_HORSE) && p->rideActor && p->rideActor->id == ACTOR_EN_HORSE) {
            const auto* horse = reinterpret_cast<const EnHorse*>(p->rideActor);
            ++mechanicMountedTicks;
            mechanicHorseAEdges += (gPlayState->state.input[0].press.button & BTN_A) != 0;
            if (mechanicLastCarrots >= 0 && horse->numBoosts < mechanicLastCarrots)
                mechanicCarrotsUsed += mechanicLastCarrots - horse->numBoosts;
            mechanicLastCarrots = horse->numBoosts;
        }
    }
    if (fixture.contains("mechanic_case") && tick % 5 == 0) {
        auto* p = GET_PLAYER(gPlayState);
        const auto* cam = GET_ACTIVE_CAM(gPlayState);
        mechanicSamples.push_back({{"tick",tick},{"position",Vec(p->actor.world.pos)},
            {"aim_pitch",p->actor.focus.rot.x},{"aim_yaw",p->actor.focus.rot.y},
            {"push_speed",p->pushedSpeed},{"push_yaw",p->pushedYaw},
            {"action",NativeSimTest_PlayerActionName(const_cast<Player*>(p))},{"frame",p->skelAnime.curFrame},
            {"animation",Animation(p->skelAnime)},{"movement_flags",p->skelAnime.movementFlags},
            {"state1",p->stateFlags1},{"state2",p->stateFlags2},{"speed",p->linearVelocity},
            {"camera_setting",cam->setting},{"camera_mode",cam->mode},{"health",gSaveContext.health},
            {"invincibility",p->invincibilityTimer}});
    }
    firstPersonFrames += GET_ACTIVE_CAM(gPlayState)->mode == CAM_MODE_FIRST_PERSON;
    worldCUpEdges += (gPlayState->state.input[0].press.button & BTN_CUP) != 0;
    if (interactionKind == "npc" && interactionTalkQ != UINT64_MAX)
        interactionWorldEdges += (gPlayState->state.input[0].press.button & (BTN_A | BTN_B)) != 0;
    if (highSteps != previousWorldHighSteps) ++highWorldTransactions;
    previousWorldHighSteps = highSteps;
    if (fixture.value("require_player_hz",false) ||
        (fixture.value("check_slope_void",false) && motionEffectChecks.voidSurfaces==0)) {
        const auto state = PlayerTemporal_Observe();
        const auto rate = PlayerTemporal_RateStatus();
        const unsigned requested = fixture.at("player_hz").get<unsigned>();
        if (!state.okay || (state.high ? rate.requested : 20) != requested)
            Fail(std::string("requested Player cadence was not admitted: ") + rate.rejection);
    }
    if(fixture.value("stalfos_loop_control",false) && tick==20 && stalfosTestActor) {
        // Exercise the real death/breakup/destruction path, not a native fault.
        stalfosTestActor->actor.colChkInfo.health=0;
        func_80862FA8(stalfosTestActor,gPlayState);
    }
    if(fixture.value("stalfos_strafe_control",false) && tick==20 && stalfosTestActor)
        func_80864158(stalfosTestActor,4.0f);
    if(fixture.value("stalfos_arena",0) && stalfosTestActor && stalfosFirstExit==UINT64_MAX) {
        if(stalfosTestActor->actionFunc==EnTest_SlashDown)
            stalfosFrames.push_back(stalfosTestActor->skelAnime.curFrame);
        else stalfosFirstExit=tick;
    }
    if (fixture.value("observe_roll",false) && rollFrames.size()<40) {
        auto* p=GET_PLAYER(gPlayState);
        rollFrames.push_back({{"tick",tick},{"action",NativeSimTest_PlayerActionName(p)},
            {"frame",p->skelAnime.curFrame},{"end",p->skelAnime.endFrame},
            {"blend",p->unk_864},{"morph",p->skelAnime.morphWeight},
            {"recovery_pose_weight",PlayerTemporal_RollRecoveryWeight()}});
    }
    ++tick;
    if (!qaControl.Commit() || qaControl.time.quanta != tick * PlayerTemporal::WorldStepQuanta)
        Fail("canonical QA commit does not match the completed transaction");
    WriteSnapshot();
    if (tick == fixture.at("ticks").get<uint64_t>()) {
        if (fixture.value("shield_transition_check",false)) {
            const auto samples=PlayerTemporal_ShieldPresentationSamples();
            std::ofstream f(output/"shield-presentation.json");
            f<<json({{"blended_pose_samples",samples}}).dump()<<'\n';
            if (fixture.value("player_hz",20)!=20 && samples<5)
                Fail("shield transition produced no intermediate presentation poses");
            if (fixture.value("player_hz",20)==20 && samples!=0)
                Fail("shield presentation changed canonical20 path");
        }
        if (fixture.value("observe_roll",false)) {
            std::ofstream f(output/"roll.json"); f<<rollFrames.dump(2)<<'\n';
            if (fixture.value("player_hz",20)!=20 &&
                (rollFrames.size()<14 || rollFrames[11]["recovery_pose_weight"].get<float>()<=0 ||
                 rollFrames[13]["recovery_pose_weight"].get<float>()!=0))
                Fail("roll recovery pose did not retain and finish its legacy handoff interval");
        }
        if (fixture.value("stalfos_restart_control",false)) {
            gSaveContext.health=0;
            if (!StalfosTemporal_ArenaRestart(gPlayState) || gPlayState->state.running ||
                gSaveContext.entranceIndex!=505 || gSaveContext.health!=gSaveContext.healthCapacity ||
                gSaveContext.respawnFlag!=0) Fail("arena death restart handoff failed");
            const auto scene=gPlayState->sceneNum;
            gPlayState->sceneNum=SCENE_LON_LON_RANCH;
            if (!StalfosTemporal_ArenaRestart(gPlayState)) Fail("arena scene escape did not restart");
            gPlayState->sceneNum=scene;
            std::ofstream f(output/"arena-restart.json");
            f<<"{\"death_restart_handoff\":true,\"scene_escape_restart_handoff\":true}\n";
        }
        FinishPlayerStatusChecks();
        FinishActionWait();
        FinishActionRate();
        FinishSpellWait();
        FinishDynaChecks();
        FinishActionClocks();
        FinishMotionEffects();
        if (fixture.value("stalfos_arena",0)) {
            const auto c=StalfosTemporal_Stats();
            json report={{"worlds",c.worlds},{"steps",c.steps},{"poses",c.poses},{"animations",c.animations},
                {"moving_steps",c.movingSteps},{"horizontal_steps",c.horizontalSteps},{"attack_samples",c.attackSamples},{"contacts",c.contacts},
                {"blocks",c.blocks},{"player_contacts",c.playerContacts},{"decisions",c.decisions},
                {"invalid",c.invalid},{"max_step",c.maxStep},{"render_commands",c.renderCommands},
                {"world_updates",stalfosWorldUpdates},{"first_slash_frames",stalfosFrames},{"first_slash_exit",stalfosFirstExit},
                {"enemy_health_start",stalfosInitialHealth},{"enemy_health_end",stalfosTestActor?stalfosTestActor->actor.colChkInfo.health:-1},
                {"player_health_start",stalfosPlayerHealth},{"player_health_end",gSaveContext.health}};
            report["culled_poses"]=c.culledPoses;
            report["arena_rounds"]=c.arenaRounds;
            report["break_pose_refreshes"]=c.breakPoseRefreshes;
            report["arena_destroyed"]=c.arenaDestroyed;
            report["arena_attempts"]=c.arenaAttempts;
            report["arena_no_ground"]=c.arenaNoGround;
            report["arena_spawn_failed"]=c.arenaSpawnFailed;
            report["arena_blocked"]=c.arenaBlocked;
            report["arena_floor"]=c.arenaFloor;
            report["arena_floor_delta"]=c.arenaFloorDelta;
            report["arena_normal"]=c.arenaNormal;
            report["arena_floor_type"]=c.arenaFloorType;
            report["arena_floor_property"]=c.arenaFloorProperty;
            {std::ofstream f(output/"stalfos.json");f<<report.dump(2)<<'\n';}
            if (fixture.value("stalfos_force_culled",false) && c.culledPoses<c.worlds*5)
                Fail("culled Stalfos lost high-rate pose opportunities");
            const auto mode=fixture.value("stalfos_contact_mode",std::string());
            if(fixture.value("stalfos_loop_control",false) && c.arenaRounds<2)
                Fail("outdoor Stalfos did not restart after death");
            if(stalfosFirstExit==UINT64_MAX || stalfosFrames.empty() ||
                stalfosWorldUpdates<(fixture.value("stalfos_loop_control",false) ? 20 :
                    std::min<uint64_t>(100,fixture.at("ticks").get<uint64_t>())))
                Fail("Stalfos scene coverage incomplete");
            if(mode=="block" && (c.blocks==0 || gSaveContext.health!=stalfosPlayerHealth))
                Fail("Stalfos shield block failed");
            if(mode=="counter" && (c.playerContacts==0 || !stalfosTestActor ||
                stalfosTestActor->actor.colChkInfo.health>=stalfosInitialHealth))
                Fail("Stalfos did not consume Player sword damage");
            if(fixture.value("stalfos_hz",0) && (c.invalid || c.steps<c.worlds*5 || c.poses<c.worlds*5 ||
                c.movingSteps<20 || (mode!="counter" && c.attackSamples==0))) Fail("Stalfos high-rate coverage incomplete: "+report.dump());
        }
        FinishPlayerItemTimerChecks();
        FinishDoorLockChecks();
        if (performanceOnly) {
            const double fps = frameTiming.intervals * 1000.0 / frameTiming.totalMs;
            const double steadyFps = steadyFrameTiming.intervals * 1000.0 / steadyFrameTiming.totalMs;
            const bool requiresCurrentPlayerPose = fixture.value("player_hz", 20) != 20;
            const bool passed = steadyFrameTiming.intervals >= 6000 && steadyFps >= 119.5 &&
                steadyFrameTiming.Percentile(0.99) <= 9.5 &&
                steadyFrameTiming.worstWindowMs <= 1010.0 && steadyFrameTiming.slowFrames == 0 &&
                renderedFrames == renderAttempts &&
                (!requiresCurrentPlayerPose || currentPlayerPoseSubmissions == renderAttempts);
            json report = {{"status",passed ? "pass" : "fail"},{"mode","performance-only"},
                {"semantic_qualification",false},{"audio","ordinary-device-worker"},
                {"fixture",fixture},{"fps",fps},{"intervals",frameTiming.intervals},
                {"steady_state",{{"fps",steadyFps},{"intervals",steadyFrameTiming.intervals},
                    {"p99_ms",steadyFrameTiming.Percentile(.99)},{"max_ms",steadyFrameTiming.maximumMs},
                    {"over_12_5_ms",steadyFrameTiming.slowFrames},
                    {"worst_120_frame_ms",steadyFrameTiming.worstWindowMs},
                    {"excluded_compile_and_recovery_intervals",steadyFrameTiming.excluded}}},
                {"shader_compilation",{{"count",measuredShaderCompilations},{"milliseconds",measuredShaderCompileMs},
                    {"cache_hits",measuredShaderCacheHits},{"excluded_from_steady_state",true}}},
                {"timing_point","present-return-before-queue-wait"},
                {"over_12_5_ms_by_player_offset",frameTiming.slowByOffset},
                {"completion_fps",completionTiming.intervals*1000.0/completionTiming.totalMs},
                {"completion_p99_ms",completionTiming.Percentile(.99)},
                {"input_to_present_ms",{{"average",totalInputToPresentMs/std::max(uint64_t(1),inputToPresentSamples)},
                    {"maximum",maxInputToPresentMs},{"over_8_333_ms",inputToPresentOverBudget},
                    {"samples",inputToPresentSamples},{"physical_scanout_measured",false}}},
                {"elapsed_seconds",frameTiming.totalMs/1000},{"p50_ms",frameTiming.Percentile(.5)},
                {"p95_ms",frameTiming.Percentile(.95)},{"p99_ms",frameTiming.Percentile(.99)},
                {"max_ms",frameTiming.maximumMs},{"over_12_5_ms",frameTiming.slowFrames},
                {"maximum_frame_index",frameTiming.maximumFrame},
                {"worst_120_frame_ms",frameTiming.worstWindowMs},{"windows",frameTiming.windows},
                {"rendered",renderedFrames},{"dropped",renderAttempts-renderedFrames},
                {"current_player_pose_submissions",currentPlayerPoseSubmissions},
                {"requires_current_player_pose",requiresCurrentPlayerPose},
                {"player_steps",highSteps},{"world_ticks",tick},
                {"vsync",CVarGetInteger(CVAR_VSYNC_ENABLED,1)},
                {"thresholds",{{"apply_to","steady_state"},{"min_intervals",6000},{"min_fps",119.5},{"max_p99_ms",9.5},{"max_120_frame_ms",1010},
                    {"max_frames_over_12_5_ms",0},{"max_drops",0}}}};
            if (!passed) { json tail=json::array();frameTiming.tail.Visit([&](double ms){tail.push_back(ms);});
                report["last_64_intervals_ms"] = tail; }
            { std::ofstream file(output/"performance.json");file << report.dump(2) << '\n'; }
            OTRAudio_Exit();
            std::exit(passed ? 0 : 2);
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - renderStart).count();
        { std::ofstream file(output / "presentation-receipt.json");
          file << json{{"attempts",renderAttempts},{"rendered",renderedFrames},
              {"dropped",renderAttempts-renderedFrames},{"elapsed_seconds",seconds},
              {"current_player_pose_submissions",currentPlayerPoseSubmissions},
              {"first_person_world_frames",firstPersonFrames},{"world_c_up_edges",worldCUpEdges}}.dump(2) << '\n'; }
        if (fixture.value("require_first_person",false) && (!firstPersonFrames || worldCUpEdges != 1))
            Fail("C-Up edge did not reach exactly one world opportunity and enter first person");
        if (detailedSnapshots) snapshots.flush();
        if (verbose) trace.flush();
        if (observeTemporal) {
            temporalSnapshots.flush();
            std::ofstream diagnostics(output / "temporal-result.json");
            diagnostics << json{{"schema",1},{"status","pass"},{"ticks",tick},
                {"single_step",singleStepControl},{"qa_holds",qaHolds},{"qa_commands",qaSequence},
                {"player_commands",qaPlayerCommands},
                {"canonical_time_q",qaControl.time.quanta},{"canonical_transaction_id",qaControl.transactionId},
                {"final",PlayerTemporal_Inspect()}}.dump(2) << '\n';
            if (!diagnostics) Fail("cannot write temporal completion receipt");
        }
        if (fixture.value("observe_live_indicators",false)) {
            const uint32_t newBits = PLAYER_STATE2_NAVI_ALERT | PLAYER_STATE2_IDLE_FIDGET;
            if (fixture.value("require_live_indicators",false) &&
                ((liveState2Seen & newBits) != newBits || !liveHintSamples || !liveStatusFrames))
                Fail("ordinary hint/fidget coverage absent: " + std::to_string(liveState2Seen));
            // Pure bit-policy checks: no mutation of live Player or gameplay.
            Player probe = *GET_PLAYER(gPlayState);
            const uint32_t allowed = PLAYER_STATE2_CAN_ACCEPT_TALK_OFFER | PLAYER_STATE2_FOOTSTEP |
                PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET | PLAYER_STATE2_LOCK_ON_WITH_SWITCH |
                PLAYER_STATE2_NAVI_ACTIVE | PLAYER_STATE2_SWORD_LUNGE | newBits;
            for (unsigned bit=0;bit<32;++bit) {
                probe.stateFlags2 = 1u << bit;
                if (Player_PoseUnexpectedState2(&probe) != ((1u << bit) & ~allowed))
                    Fail("stateFlags2 blocker mask was broadened");
            }
            char observed[2048], rejected[2048];
            Player_FormatState2(liveState2Seen, liveState2Seen & newBits, observed, sizeof(observed));
            Player_FormatState2(PLAYER_STATE2_CRAWLING, PLAYER_STATE2_CRAWLING, rejected, sizeof(rejected));
            if (!strstr(rejected,"stateFlags2=0x00040000 unexpected=0x00040000 [PLAYER_STATE2_CRAWLING]"))
                Fail("symbolic stateFlags2 diagnostic mismatch");
            json samples=json::array();
            for (const auto& pair:liveState2Samples) samples.push_back({{"stateFlags2",pair.first},{"samples",pair.second}});
            const auto status=PlayerTemporal_RateStatus();
            std::ofstream file(output/"live-indicators.json");
            file << json{{"status","pass"},{"observed_masks",samples},{"formerly_rejected",observed},
                {"hint_samples",liveHintSamples},{"host_frame_status_checks",liveStatusFrames},
                {"bit_policy_checks",32},{"requested",status.requested},{"effective",status.effective},
                {"fallback_latched",status.fallbackLatched},{"rejection",status.rejection},
                {"diagnostic_control",rejected}}.dump(2) << '\n';
        }
        if (verifyPresentationPurity) WritePurity("pass");
        if (AuditCanonicalContacts()) {
            std::ofstream file(output/"player-phase-contact.json");
            file << json{{"format","player-phase-contact-v1"},{"fixture",fixture},
                {"boundaries",phaseContactBoundaries}}.dump() << '\n';
            if (!file) Fail("phase/contact receipt write failed");
        }
        if (fixture.value("validate_contact_bridge",false)) {
            auto bridge = PlayerContact::Inspect();
            const bool invalidation = fixture.value("bridge_invalidate_target",false);
            if (bridge.at("pending") != 0 || bridge.at("reserved") != 1 ||
                bridge.at("committed") != (invalidation ? 0 : 1) ||
                bridge.at("rejected") != (invalidation ? 1 : 0) || bridgeCuts != (invalidation ? 0 : 1))
                Fail("bridge reservation/response lifecycle mismatch: " + bridge.dump());
            if (!invalidation && bridgeTargetUpdates != tick) Fail("sign update cadence differs from world20");
            if (invalidation && (!bridgeInvalidated || PlayerTemporal_Inspect().at("effective_player_hz") != 20))
                Fail("target invalidation did not fail closed to canonical Player");
            if (!invalidation && reinterpret_cast<EnKanban*>(bridgeTarget)->cutType !=
                fixture.at("expected_sign_cut_type").get<int>()) Fail("unexpected producing-animation sign cut");
            if (fixture.value("bridge_z_target",false) && !targetChecks) Fail("bridged target hold coverage missing");
            if (fixture.value("bridge_require_duplicates",false) && bridge.at("duplicates") == 0)
                Fail("sustained contact did not exercise deduplication");
            bridge["target_updates"] = bridgeTargetUpdates; bridge["cuts"] = bridgeCuts;
            bridge["cut_type"] = bridgeTarget ? reinterpret_cast<EnKanban*>(bridgeTarget)->cutType : -1;
            bridge["status"] = "pass";
            std::ofstream file(output/"contact-bridge.json"); file << bridge.dump(2) << '\n';
        }
        if (highSteps) {
            if (posePending || poseChecks!=highSteps) Fail("incomplete high-rate pose coverage");
            if (fixture.contains("expected_attack_edge_q") && (warmChecks<2 || sweepChecks<2)) Fail("sword history coverage missing");
            if (fixture.value("spawn_distant_target",false) && (!targetChecks || !targetWorldUpdates)) Fail("held target coverage missing");
            size_t expectedControls=0;
            const unsigned q=120/NativeSimTest_ConfigInt("player_hz",20);
            for (const auto& event:fixture.at("input")) {
                const uint64_t n=event.at("time_num").get<uint64_t>()*120,d=event.at("time_den").get<uint64_t>();
                if (n%d || event.at("buttons")!=0 || (event.value("stick_x",0)==0 && event.value("stick_y",0)==0)) continue;
                if ((n/d)%6 && (((n/d+q-1)/q)*q)%6) ++expectedControls;
            }
            if (!fixture.contains("expected_fallback_edge_q") && controlChecks.size()!=expectedControls) Fail("missing intermediate control coverage");
            const bool expectsMovement = std::any_of(controlChecks.begin(), controlChecks.end(),
                [](const json& check) { return check.at("movement_expected").get<bool>(); });
            if (expectsMovement && (!motionChecks || !movingChecks || !animationChecks || !cameraChanges))
                Fail("movement/control coverage missing");
        }
        { std::ofstream file(output/"player-validation.json");
          file<<json{{"status","pass"},{"steps",highSteps},{"poses",poseChecks},{"horizontal_map_checks",motionChecks},
              {"moving_intervals",movingChecks},{"animation_phase_checks",animationChecks},
              {"intermediate_camera_changes",cameraChanges},{"controls",controlChecks},{"held_target_checks",targetChecks},
              {"warm_samples",warmChecks},{"valid_sweeps",sweepChecks},{"no_motion_samples",noMotionChecks},
              {"high_world_transactions",highWorldTransactions},
              {"held_target_world_updates",targetWorldUpdates},{"ring_bytes",sizeof(stepRing)}}.dump(2)<<'\n';
          if (!file) Fail("Player validation receipt write failed"); }
        if (!interactionKind.empty()) {
            const auto contacts = PlayerContact::Inspect().at("world_contacts").get<uint64_t>() - interactionContacts;
            const auto cuts = gSaveContext.ship.stats.count[COUNT_BUSHES_CUT] - interactionBushes;
            const bool passed = interactionKind == "npc" ?
                interactionTalkQ == fixture.at("expected_talk_q").get<uint64_t>() &&
                interactionTextboxQ != UINT64_MAX && interactionWorldEdges == fixture.value("expected_message_edges", 3u) && interactionUpdates == tick :
                interactionKind == "body" ? mechanicBodySteps > 0 && mechanicBodyMaxStep < 1.5f &&
                    PlayerContact::Inspect().at("body_constraints").get<uint64_t>() > 0 :
                    contacts > 0 && cuts == 1 && interactionDestroyed;
            const json report = {{"kind",interactionKind},{"passed",passed},{"target_updates",interactionUpdates},
                {"world_ticks",tick},{"talk_response_q",interactionTalkQ},{"textbox_open_q",interactionTextboxQ},
                {"message_world_edges",interactionWorldEdges},{"closing_substep_checks",dialogueClosingChecks},
                {"body_contacts",interactionOverlaps},{"sword_contacts",contacts},{"grass_cuts",cuts},
                {"constrained_body_steps",mechanicBodySteps},{"max_blocked_step",mechanicBodyMaxStep},
                {"max_step_observation",mechanicBodyPeak},
                {"target_destroyed",interactionDestroyed}};
            { std::ofstream file(output / "world-interaction.json"); file << report.dump(2) << '\n'; }
            if (!passed) Fail("world interaction coverage failed: " + report.dump());
        }
        if (fixture.value("test_talk_closing", false) &&
            (!dialogueCloseInjected || !dialogueClosingChecks ||
             gPlayState->msgCtx.msgMode != MSGMODE_NONE || (GET_PLAYER(gPlayState)->actor.flags & ACTOR_FLAG_TALK)))
            Fail("closing fixture did not finish without reopening dialogue");
        if (fixture.value("observe_aim_presentation", false)) {
            const auto stats = FrameInterpolation_GetFirstPersonStats();
            std::ofstream file(output / "aim-presentation.json");
            file << json{{"frames",stats.frames},{"blended_frames",stats.blendedFrames},
                {"matrix_checks",stats.matrices},{"attachment_matrix_checks",stats.attachmentMatrices},
                {"last_attachment_matrices",stats.lastAttachmentMatrices},{"max_view_space_error",stats.maxViewError}}.dump(2) << '\n';
            if (stats.blendedFrames < 60 || stats.matrices < 100 || stats.maxViewError > 0.01f)
                Fail("first-person presentation did not preserve its camera-space pose");
            if (fixture.value("aim_release", false) && (!stats.attachmentMatrices || stats.lastAttachmentMatrices))
                Fail("held arrow presentation was not captured then released");
        }
        const auto assertionResult=onlineAssertions->Result();
        if (fixture.contains("mechanic_case")) {
            std::ofstream file(output / "mechanic-check.json");
            const auto skinStats = FrameInterpolation_GetSkinStats();
            file << json{{"case",fixture.at("mechanic_case")},{"initial_health",mechanicHealth},
                {"skin_recorded_limbs",skinStats.recordedLimbs},
                {"skin_intermediate_limbs",skinStats.intermediateLimbs},
                {"skin_intermediate_vertices",skinStats.changedVertices},
                {"mounted_ticks",mechanicMountedTicks},{"horse_a_edges",mechanicHorseAEdges},
                {"carrots_used",mechanicCarrotsUsed},
                {"water_ticks",mechanicWaterTicks},{"current_ticks",mechanicCurrentTicks},
                {"aim_response_checks",aimResponseChecks},
                {"held_bomb_ticks",mechanicHeldBombSteps},{"held_explosion_ticks",mechanicHeldExplosionSteps},
                {"damage_ticks",mechanicDamageSteps},{"crawl_ticks",mechanicCrawlSteps},
                {"crawl_camera_ticks",mechanicCrawlCameraSteps},
                {"final_health",gSaveContext.health},{"samples",mechanicSamples},{"spin_events",spinEvents}}.dump(2) << '\n';
            file.close();
            const auto kind = fixture.at("mechanic_case").get<std::string>();
            if (kind == "spin") {
                bool released = false;
                for (const auto& event : spinEvents) if (event.at("spin").get<bool>()) {
                    if (!released && fixture.contains("spin_release_q") &&
                        event.at("q") != fixture.at("spin_release_q"))
                        Fail("spin release did not execute on the expected Player opportunity");
                    released = true;
                    if (fixture.contains("spin_charge_max") && event.at("charge").get<float>() > fixture.at("spin_charge_max").get<float>())
                        Fail("early spin crossed the magical charge threshold");
                }
                if (released != fixture.value("expect_spin",true))
                    Fail(released ? "spin fixture accepted a too-early release" : "spin fixture did not execute a released spin");
            }
            if (fixture.contains("aim_response_q") && aimResponseChecks != 1)
                Fail("missing first-step aiming response coverage");
            if (kind == "swim" && mechanicWaterTicks < 20) Fail("swim fixture did not stay in water");
            if (kind == "mount" && mechanicMountedTicks == 0) Fail("A press did not mount the offered horse");
            if (kind == "horse" && (mechanicMountedTicks == 0 || mechanicHorseAEdges != 1 || mechanicCarrotsUsed != 1))
                Fail("mounted horse did not receive exactly one A edge and consume one carrot");
            if (kind == "horse" && fixture.value("presentation_fps", 20) > 20 &&
                skinStats.changedVertices == 0)
                Fail("horse presentation did not produce intermediate skin vertices");
            if (kind == "held-bomb" && (mechanicHeldBombSteps < 50 || mechanicHeldExplosionSteps == 0 ||
                mechanicHealth-gSaveContext.health != 8 || mechanicDamageSteps == 0))
                Fail("held bomb must explode overhead and deliver one health loss with damage reaction");
            if (kind == "crawl" && (mechanicCrawlSteps == 0 || mechanicCrawlCameraSteps == 0 ||
                GET_PLAYER(gPlayState)->actor.world.pos.z < 1400 ||
                GET_PLAYER(gPlayState)->stateFlags2 & PLAYER_STATE2_CRAWLING))
                Fail("crawl must enter its camera mode, traverse the tunnel, and exit");
        }
        { std::ofstream file(output/"assertions.json");file<<assertionResult.dump(2)<<'\n';
          if (!file) Fail("assertion receipt write failed"); }
        if (assertionResult.at("status")!="pass") Fail("fixture behavior assertion failed");
        { std::ofstream file(output/"checkpoints.json");
          file<<json{{"format","semantic-sha256-v1"},{"fixture",fixture},{"snapshots",checkpoints},
              {"reference_compared",!expectedCheckpoints.is_null()},{"boundary_ring_capacity",4},
              {"substep_ring_capacity",32}}.dump()<<'\n';
          if (!file) Fail("checkpoint receipt write failed"); }
        json result = {{"schema", 1}, {"status", "pass"}, {"fixture_id", fixture.at("id")},
            {"ticks_completed", tick}, {"rate_hz", 20}, {"time_q", tick * 6},
            {"engine_frames", engineFrames}, {"setup_ticks", setupFrames},
            {"fixture", fixture},
            {"diagnostics",{{"mode",detailedSnapshots?"explicit-full":"compact"},
                {"semantic_hash_format","semantic-sha256-v1"},{"online_assertions",true}}},
            {"configuration", {{"interpolation_fps", CVarGetInteger(CVAR_SETTING("InterpolationFPS"), 20)},
                {"match_refresh_rate", CVarGetInteger(CVAR_SETTING("MatchRefreshRate"), 0)},
                {"mouse", CVarGetInteger(CVAR_SETTING("EnableMouse"), 0)},
                {"time_sync", CVarGetInteger(CVAR_CHEAT("TimeSync"), 0)}}},
            {"snapshot_phase", "after_graphics_audio_and_savestate_requests"},
            {"audio_policy", "synchronous_528_sample_blocks_legacy_grouping"},
            {"coverage", "semantic_v1_player_camera_base_actors_rng_audio_no_generic_actor_actions"}};
        auto temp = output / "result.pending.json";
        { std::ofstream file(temp); file << result.dump(2) << '\n'; if (!file) Fail("manifest write failed"); }
        std::filesystem::rename(temp, output / "result.json");
        std::exit(0);
    }
}
