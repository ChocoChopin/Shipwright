#include "PlayerTemporal.h"
#include "PlayerTemporalCore.hpp"
#include <cstring>
#include <cmath>
extern "C" {
#include "global.h"
#include "regs.h"
uint64_t GetPerfCounter(void);
uint64_t GetFrequency(void);
}
namespace {
using namespace PlayerTemporal;
Lifecycle life;
Capability capability;
Player* boundPlayer = nullptr; // lookup only; never serialized or used as event identity
PlayerStepContext playerStep;
WorldStepContext worldStep;
SimTime worldTime{}, playerTime{};
uint64_t worldSteps = 0, playerSteps = 0, poses = 0, liveInputSequence = 0;
uint64_t actionGeneration = 0, equipmentGeneration = 0;
bool worldOpen = false, playerOpen = false, okay = true;
bool admissionKnown = false, poseAdmitted = false;
bool equipmentKnown = false;
bool suspended = false, inputRequested = false;
uint32_t equipment = 0;
OpportunityCursor worldGuard;
InputTimeline inputs;
InputEvent lastInput{};
struct AnimationInterval {
    AnimationEvents events;
    std::array<uint64_t, 16> markers{};
    unsigned markerCount = 0;
    float from = 0, to = 0;
};
AnimationInterval animationIntervals[2];
AnimationInterval* AnimationState(const SkelAnime* animation) {
    if (!boundPlayer) return nullptr;
    if (animation == &boundPlayer->skelAnime) return &animationIntervals[0];
    if (animation == &boundPlayer->upperSkelAnime) return &animationIntervals[1];
    return nullptr;
}
void Check(bool value) { okay &= value; } // diagnostic failure only; no native fault / gameplay changes
void ResetInput() { inputs.Reset(life.identity); lastInput = {}; inputRequested = false; }
void InvalidateScope() {
    Check(life.Invalidate()); ResetInput();
    animationIntervals[0] = {}; animationIntervals[1] = {};
}
void ResetScope() {
    ResetInput(); admissionKnown = equipmentKnown = poseAdmitted = suspended = false;
    // Player lifetime changes cancel only its in-flight context. World work may
    // already be open in the same transaction and must still commit its interval.
    playerStep = {}; playerOpen = false;
    animationIntervals[0] = {}; animationIntervals[1] = {};
}
}
extern "C" void PlayerTemporal_SceneInit() {
    Check(life.Scene()); boundPlayer = nullptr; worldOpen = playerOpen = false;
    ResetScope(); worldStep = {}; worldGuard.Reset({life.identity.scene,0,0}, Domain::World);
}
extern "C" void PlayerTemporal_ActorCreated(Actor* actor) {
    if (actor->id != ACTOR_PLAYER) return;
    boundPlayer = reinterpret_cast<Player*>(actor);
    Check(life.CreatePlayer()); ResetScope();
}
extern "C" void PlayerTemporal_ActorDestroyed(Actor* actor) {
    if (actor != reinterpret_cast<Actor*>(boundPlayer)) return;
    Check(life.DestroyPlayer()); boundPlayer = nullptr; ResetScope();
}
extern "C" void PlayerTemporal_BeginFrame() { worldOpen = playerOpen = false; }
extern "C" void PlayerTemporal_EndFrame() {
    if (worldOpen) { worldTime = worldStep.endTime; worldSteps = worldStep.worldStepId; }
    if (playerOpen) { Check(Add(playerTime,{WorldStepQuanta},playerTime)); playerSteps = playerStep.playerStepId; }
    worldOpen = playerOpen = false;
}
extern "C" void PlayerTemporal_PlayBoundary(PlayState* play) {
    if (!boundPlayer || GET_PLAYER(play) != boundPlayer) return;
    // These states can skip the draw callback entirely. Invalidate only sidecar
    // ownership on entry; legacy joint/weapon history keeps its original resets.
    const bool blocked = play->pauseCtx.state || play->pauseCtx.debugState || play->frameAdvCtx.enabled ||
        play->actorCtx.freezeFlashTimer || play->haltAllActors || play->transitionTrigger || play->transitionMode ||
        play->gameOverCtx.state != GAMEOVER_INACTIVE || !boundPlayer->actor.draw ||
        boundPlayer->actor.freezeTimer || (boundPlayer->stateFlags1 & PLAYER_STATE1_DEAD);
    if (blocked && !suspended) {
        InvalidateScope(); poseAdmitted = false; admissionKnown = true;
    }
    suspended = blocked;
}
extern "C" void PlayerTemporal_Sample(const char* site, PlayState* play) {
    if (!play) return;
    if (std::strcmp(site, "collision.begin") == 0 && R_UPDATE_RATE == 3) {
        Check(!worldOpen && worldSteps != UINT64_MAX);
        worldStep = {WorldStepQuanta, worldTime, {}, worldSteps+1, life.identity.scene};
        Check(Add(worldTime,{WorldStepQuanta},worldStep.endTime));
        // World ownership depends on scene, not Player profile/reset generations.
        Identity worldOwner{life.identity.scene,0,0};
        if (worldSteps == 0) worldGuard.Reset(worldOwner,Domain::World);
        // SceneInit resets the guard; bind its scene-only identity there as well.
        Check(ConsumeWorld(worldGuard,worldStep,worldOwner,worldTime));
        worldOpen = true;
    } else if (std::strcmp(site,"actor.update.begin") == 0 && worldOpen && GET_PLAYER(play) == boundPlayer) {
        Check(!playerOpen && playerSteps != UINT64_MAX);
        uint32_t current = uint32_t(boundPlayer->currentShield) | (uint32_t(boundPlayer->currentBoots)<<8) |
                           (uint32_t(boundPlayer->heldItemAction & 0xff)<<16) | (uint32_t(boundPlayer->currentTunic)<<24);
        if (equipmentKnown && current != equipment) {
            ++equipmentGeneration; InvalidateScope();
        }
        equipment = current; equipmentKnown = true;
        // Context intervals use shared simulation availability time. Elapsed
        // Player time separately counts only transactions that update Player.
        playerStep = {SimulationRate::Hz20,WorldStepQuanta,worldTime,worldStep.endTime,playerSteps+1,life.identity};
        playerOpen = true;
        if (inputRequested) {
            InputEvent event;
            while (inputs.ConsumeForPlayer(playerStep,event)) lastInput = event;
            inputRequested = false;
        }
    } else if (std::strcmp(site,"actor.update.end") == 0 && playerOpen) {
        Check(life.animation.Advance());
    }
}
extern "C" void PlayerTemporal_ActionChanged(Player* player) {
    if (player != boundPlayer) return;
    ++actionGeneration; life.attack.End();
}
extern "C" void PlayerTemporal_AttackStarted(Player* player) {
    if (player == boundPlayer) Check(life.attack.Begin());
}
extern "C" void PlayerTemporal_MeleeWindow(Player* player, int active) {
    if (player == boundPlayer) Check(life.attack.Window(active > 0));
}
extern "C" void PlayerTemporal_AnimationChanged(SkelAnime* animation) {
    if (boundPlayer && animation == &boundPlayer->skelAnime) Check(life.animation.Change());
    if (auto* state = AnimationState(animation)) {
        Check(state->events.Change()); Check(state->events.Advance());
        state->markerCount = 0;
        const unsigned quanta = PlayerTemporal_HighAnimationQuanta(animation);
        // A newly selected frame has the legacy entry-marker opportunity. Later
        // intervals use the actual before/after frames, including endpoint clamps.
        state->to = animation->curFrame;
        state->from = state->to - animation->playSpeed * (quanta ? quanta * 0.25f : R_UPDATE_RATE * 0.5f);
    }
}
extern "C" unsigned PlayerTemporal_HighStepQuanta(const Player* player) {
    return playerOpen && player == boundPlayer && playerStep.rate != SimulationRate::Hz20 ? playerStep.stepQuanta : 0;
}
extern "C" unsigned PlayerTemporal_HighAnimationQuanta(const SkelAnime* animation) {
    return AnimationState(animation) ? PlayerTemporal_HighStepQuanta(boundPlayer) : 0;
}
extern "C" void PlayerTemporal_AnimationAdvanced(SkelAnime* animation, float previousFrame) {
    auto* state = AnimationState(animation);
    if (!state) return;
    Check(state->events.Advance());
    state->from = previousFrame; state->to = animation->curFrame;
    if (animation->playSpeed > 0 && state->to < state->from) state->to += animation->animLength;
    if (animation->playSpeed < 0 && state->to > state->from) state->to -= animation->animLength;
}
extern "C" int PlayerTemporal_AnimationMarker(SkelAnime* animation, float marker) {
    auto* state = AnimationState(animation);
    if (!state || !PlayerTemporal_HighAnimationQuanta(animation)) return 0;
    const auto q16 = [](float frame) { return static_cast<int64_t>(std::llround(double(frame) * 65536.0)); };
    MarkerRange crossings;
    if (!Crossings(q16(state->from), q16(state->to), q16(marker), q16(animation->animLength), crossings)) {
        Check(false); return 0;
    }
    return crossings.count != 0;
}
extern "C" int PlayerTemporal_ConsumeAnimationMarker(SkelAnime* animation, float marker, uint64_t eventId) {
    if (!PlayerTemporal_AnimationMarker(animation, marker)) return 0;
    auto* state = AnimationState(animation);
    unsigned index = 0;
    while (index < state->markerCount && state->markers[index] != eventId) ++index;
    if (index == state->markerCount) {
        if (index == state->markers.size()) { Check(false); return 0; }
        state->markers[state->markerCount++] = eventId;
    }
    // Different logical consumers at the same authored frame are distinct. The
    // frame query itself is pure; only an event owner consumes its opportunity.
    return state->events.Consume(index, state->events.generation, state->events.interval, {0, 1, 1});
}
extern "C" void PlayerTemporal_PoseAdmission(Player* player, int admitted) {
    if (player != boundPlayer) return;
    ++poses;
    if (admissionKnown && poseAdmitted && !admitted) {
        InvalidateScope();
    }
    admissionKnown = true; poseAdmitted = admitted != 0;
}
extern "C" void PlayerTemporal_InputSample(uint64_t seq, uint64_t num, uint64_t den,
                                            unsigned port, uint32_t held, uint16_t press, uint16_t release) {
    if (!life.playerAlive || suspended || port >= 4) return;
    Check(inputs.Queue({life.identity,seq,num,den,worldTime,held,press,release,static_cast<uint8_t>(port)}));
}
extern "C" void PlayerTemporal_LiveInput(PadMgr* padMgr) {
    // Physical polling stays at its original cadence. Host timestamps remain
    // rational and separate from the simulation availability boundary.
    if (!life.playerAlive) return;
    uint64_t timestamp = GetPerfCounter(), frequency = GetFrequency();
    for (unsigned port=0;port<4;++port) {
        auto& in = padMgr->inputs[port];
        uint32_t changed = in.cur.button ^ in.prev.button;
        if (changed) PlayerTemporal_InputSample(++liveInputSequence,timestamp,frequency,port,in.cur.button,
            static_cast<uint16_t>(changed & in.cur.button),static_cast<uint16_t>(changed & in.prev.button));
    }
}
extern "C" void PlayerTemporal_InputConsumed() {
    // PadMgr still owns real input. This only marks when observed acquisition
    // may be associated with the next actual Player update, including no-step holds.
    inputRequested = true;
}
nlohmann::json PlayerTemporal_Inspect() {
    return {{"okay",okay},{"requested_player_hz",static_cast<unsigned>(capability.requestedPlayerRate)},
        {"effective_player_hz",static_cast<unsigned>(capability.EffectiveRate())},
        {"world_hz",capability.WorldRate()},{"player_high_rate_admitted",capability.HighRateAdmitted()},
        {"time_q",worldTime.quanta},{"player_time_q",playerTime.quanta},
        {"world_step_id",worldSteps},{"player_step_id",playerSteps},
        {"player_interval_start_q",playerStep.startTime.quanta},{"player_interval_end_q",playerStep.endTime.quanta},
        {"scene_epoch",life.identity.scene},{"player_generation",life.identity.player},
        {"scope_generation",life.identity.scope},{"player_alive",life.playerAlive},
        {"action_generation",actionGeneration},{"equipment_generation",equipmentGeneration},
        {"animation_generation",life.animation.generation},{"animation_interval",life.animation.interval},
        {"attack_epoch",life.attack.epoch},{"attack_active",life.attack.attacking},
        {"authored_hit_opportunity",life.attack.hitOpportunity},{"active_window",life.attack.activeWindow},
        {"queued_input_samples",inputs.Queued()},{"consumed_input_sequence",inputs.consumedSequence},
        {"consumed_input_edges",inputs.consumedEdges},
        {"input_consuming_player_step",inputs.consumingPlayerStep},{"suspended",suspended},
        {"last_input",{{"sequence",lastInput.sequence},{"time_num",lastInput.timeNumerator},
            {"time_den",lastInput.timeDenominator},{"available_q",lastInput.available.quanta},
            {"held",lastInput.held},{"pressed",lastInput.pressed},{"released",lastInput.released}}},
        {"pose_generation",poses},{"pose_admitted",poseAdmitted},
        {"contact_queue_count",0},{"contact_bridge_active",false}};
}
