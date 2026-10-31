#include "StalfosTemporal.h"
#include "PlayerTemporal.h"
#include "PlayerTemporalCore.hpp"
#include "PlayerContactBridge.h"
#include "PlayerMotionCore.hpp"
#include "PlayerSchedulerCore.hpp"
#include "NativeSimulationTest.h"
#include "NativeSimulationPresentation.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/controller/controldeck/ControlDeck.h>
#include <ship/controller/physicaldevice/ConnectedPhysicalDeviceManager.h>
#include <cstring>
#include <algorithm>
#include <cmath>
extern "C" {
#include "global.h"
#include "regs.h"
#include "player_step.h"
#include "player_input.h"
#include "player_animation.h"
#include "player_pose.h"
uint64_t GetPerfCounter(void);
uint64_t GetFrequency(void);
}
namespace {
using namespace PlayerTemporal;
Lifecycle life;
Capability capability;
Player* boundPlayer = nullptr; // lookup only; never serialized or used as event identity
struct RollRecovery {
    LinkAnimationHeader* animation=nullptr;
    float frame=0, speed=0, blendStart=0, blendEnd=0, weight=0;
} rollRecovery;
Actor* mountOffer = nullptr; // world-produced offer, valid only within this world transaction
int mountOfferSide = 0;
struct EarlyTalkOffer {
    Actor* actor = nullptr;
    float distance = 0;
    int exchangeItem = EXCH_ITEM_NONE;
} earlyTalkOffer; // producer precedes Player; borrowed only within this world transaction
PlayerStepContext playerStep;
WorldStepContext worldStep;
SimTime worldTime{}, playerTime{};
uint64_t worldSteps = 0, playerSteps = 0, poses = 0, liveInputSequence = 0;
uint64_t actionGeneration = 0, equipmentGeneration = 0;
bool worldOpen = false, playerOpen = false, okay = true;
bool pendingBurnFootstep = false;
// World-opportunity ID, not Player steps or elapsed quanta; zero means no fence.
uint64_t doorNotificationWorld = 0;
struct HoverReset {
    Identity identity{};
    uint64_t nextTime = 0;
    float velocity = 0;
    bool pending = false;
} hoverReset;
bool admissionKnown = false, poseAdmitted = false;
bool equipmentKnown = false;
bool suspended = false, inputRequested = false;
uint32_t equipment = 0;
OpportunityCursor worldGuard;
InputTimeline inputs;
InputEvent lastInput{};
FixedPlayerClock playerClock;
bool highWorld = false, highFallback = false;
unsigned requestedHz = 20, admittedHz = 20;
std::string latchedRejection;
std::string highRejection = "not requested";
PlayState* highPlay = nullptr;
Gfx* poseBinding = nullptr;
const PlayerPosePacket* presentationPacket = nullptr;
int poseLod = 0;
uintptr_t poseObjectSegment = 0;
Mtx* projectionKey = nullptr;
Mtx* viewingKey = nullptr;
MtxF preparedProjection{}, preparedViewing{};
bool viewPrepared = false;
uint64_t hostFrameStart = 0;
constexpr size_t PoseCommandCapacity = PLAYER_LIMB_MAX * 3 + 4;
PlayerPosePacket* intermediatePackets = nullptr;
Gfx* intermediateCommands = nullptr;
unsigned intermediatePacketIndex = 0;
struct AnimationInterval {
    AnimationEvents events;
    std::array<uint64_t, 16> markers{};
    unsigned markerCount = 0;
    float from = 0, to = 0;
    uint64_t terminalStart = 0;
    bool terminalHeld = false;
    float legacyFrame = 0;
    uint64_t nextLegacyTime = 0;
    bool legacyComplete = false;
};
AnimationInterval animationIntervals[2];
std::array<PeriodicPlayerOpportunity, PLAYER_PULSE_COUNT> legacyPulses{};
struct AngleRemainder { RateRemainder fraction; int16_t previous = 0; bool known = false; };
constexpr unsigned AngleFields = 18, ScratchAngles = 2;
std::array<AngleRemainder, AngleFields + ScratchAngles> angleRemainders{};
std::array<AngleRemainder, 2> aimRemainders{};
WorldStickGesture analogStruggle;
std::array<PlayerAngleFilter, AngleFields + ScratchAngles> angleFilters{};
unsigned AngleOwner(Player* player, int16_t* angle, unsigned scratchOwner) {
    if (scratchOwner) return scratchOwner <= ScratchAngles ? AngleFields + scratchOwner - 1 : UINT_MAX;
    const std::array<int16_t*, AngleFields> fields{&player->actor.shape.rot.y, &player->yaw, &player->unk_6C2,
        &player->headLimbRot.z, &player->upperLimbRot.x, &player->upperLimbRot.y, &player->upperLimbRot.z,
        &player->actor.focus.rot.x, &player->unk_89C, &player->unk_3BC.y, &player->headLimbRot.x,
        &player->headLimbRot.y, &player->actor.focus.rot.y, &player->actor.focus.rot.z,
        &player->upperLimbYawSecondary, &player->pushedYaw, &player->actor.world.rot.y, &player->actor.shape.rot.x};
    for (unsigned i = 0; i < fields.size(); ++i) if (fields[i] == angle) return i;
    return UINT_MAX;
}
AnimationInterval* AnimationState(const SkelAnime* animation) {
    if (!boundPlayer) return nullptr;
    if (animation == &boundPlayer->skelAnime) return &animationIntervals[0];
    if (animation == &boundPlayer->upperSkelAnime) return &animationIntervals[1];
    return nullptr;
}
unsigned firstFailureLine = 0;
void CheckAt(bool value, unsigned line) {
    if (!value && okay) firstFailureLine = line;
    okay &= value; // diagnostic failure only; no native fault / gameplay changes
}
#define Check(value) CheckAt((value), __LINE__)
void ResetInput() { inputs.Reset(life.identity); lastInput = {}; inputRequested = false; }
void ApplyHoverMotionConstraint(const Player* player, uint64_t start, PlayerMotion& state) {
    if (!hoverReset.pending || player != boundPlayer || !(hoverReset.identity == life.identity) ||
        hoverReset.nextTime != start ||
        player->actor.velocity.y != hoverReset.velocity || !std::isfinite(state.gravity)) return;
    // The real hover branch authored this reset for the immediately next step.
    // Idle/run changes can retain it; a later velocity override supersedes it.
    // Its canonical post-gravity speed is a constraint, not a free-fall segment.
    // Keep current gravity/clamping and the usual XZ/world-correction integration.
    state.velocity[1] = std::max(state.terminalVelocity, hoverReset.velocity + state.gravity);
    state.gravity = 0.0f;
}
void InvalidateScope(bool preserveInput = false) {
    mountOffer = nullptr;
    earlyTalkOffer = {};
    hoverReset = {};
    PlayerContact::Invalidate();
    admittedHz = 20;
    Check(life.Invalidate());
    // A high-rate profile/equipment change is not a Player lifetime change.
    // Preserve pending logical events, including a just-acquired world edge.
    if (preserveInput || requestedHz != 20) Check(inputs.RebindScope(life.identity));
    else ResetInput();
    PlayerCamera_ResetPolicy();
    animationIntervals[0] = {}; animationIntervals[1] = {};
    legacyPulses = {};
    angleRemainders = {}; aimRemainders = {}; analogStruggle = {};
    angleFilters = {};
}
void ResetScope() {
    mountOffer = nullptr;
    earlyTalkOffer = {};
    pendingBurnFootstep = false;
    doorNotificationWorld = 0;
    hoverReset = {};
    PlayerContact::Invalidate();
    ResetInput(); admissionKnown = equipmentKnown = poseAdmitted = suspended = false;
    PlayerCamera_ResetPolicy();
    // Player lifetime changes cancel only its in-flight context. World work may
    // already be open in the same transaction and must still commit its interval.
    playerStep = {}; playerOpen = false;
    animationIntervals[0] = {}; animationIntervals[1] = {};
    legacyPulses = {};
    angleRemainders = {}; aimRemainders = {}; analogStruggle = {};
    angleFilters = {};
    admittedHz = 20; highRejection = "awaiting Player admission"; latchedRejection.clear();
    highWorld = highFallback = false; highPlay = nullptr; poseBinding = nullptr; presentationPacket = nullptr; viewPrepared = false;
    playerClock = FixedPlayerClock{};
}
void CaptureControlView() {
    // Cameras serviced only by the world dispatcher need normal render
    // interpolation. Replacing their matrices with a held world pose on every
    // render frame produces visible 20-Hz camera jumps even at high render FPS.
    if (PlayerTemporal_UnrestrictedPilot() && PlayerCamera_ProfileRejection(highPlay)) {
        viewPrepared = false;
        return;
    }
    auto& view = highPlay->view;
    Mtx projection;
    uint16_t normal;
    const float aspect = float(view.viewport.rightX - view.viewport.leftX) /
                         float(view.viewport.bottomY - view.viewport.topY);
    guPerspective(&projection, &normal, view.fovy, aspect, view.zNear, view.zFar, view.scale);
    Matrix_MtxToMtxF(&projection, &preparedProjection);
    guLookAtF(preparedViewing.mf, view.eye.x, view.eye.y, view.eye.z,
        view.lookAt.x, view.lookAt.y, view.lookAt.z, view.up.x, view.up.y, view.up.z);
    viewPrepared = true;
}
void CommitHighPlayer() {
    StalfosTemporal_Contacts(highPlay, life.attack.epoch, life.attack.hitOpportunity);
    PlayerContact::Detect(highPlay, playerStep, life.attack);
    Check(playerClock.CommitPlayer());
    Check(Add(playerTime, {playerStep.stepQuanta}, playerTime));
    playerSteps = playerStep.playerStepId;
    playerOpen = false;
    NativeSimTest_PlayerStepCommitted(highPlay);
}
void RevokeHigh(const char* reason) {
    earlyTalkOffer = {};
    hoverReset = {};
    PlayerContact::Invalidate();
    highRejection = latchedRejection = reason;
    admittedHz = 20;
    Check(playerClock.RevokeAdmission()); highFallback = true;
    Check(life.Invalidate()); Check(inputs.RebindScope(life.identity));
    PlayerCamera_ResetPolicy();
    animationIntervals[0] = {}; animationIntervals[1] = {};
    legacyPulses = {}; angleRemainders = {}; aimRemainders = {}; angleFilters = {}; analogStruggle = {};
}
}
extern "C" int PlayerTemporal_UnrestrictedPilot() {
    if (NativeSimTest_IsEnabled()) return NativeSimTest_IsMeasuring() &&
        NativeSimTest_ConfigInt("unrestricted_player",0) && NativeSimTest_ConfigInt("player_hz",20) != 20;
    // Asset-free CLI checks run before InitOTR creates the settings context.
    // No live pilot selection exists during that bootstrap interval.
    if (!Ship::Context::GetRawInstance()) return false;
    const auto hz = CVarGetInteger(PLAYER_EXPERIMENTAL_HZ_CVAR,20);
    return hz == 60 || hz == 120;
}
extern "C" void PlayerTemporal_ContractFailure() { Check(false); }
extern "C" void PlayerTemporal_SceneInit() {
    rollRecovery={};
    StalfosTemporal_Reset();
    PlayerContact::Scene();
    Check(life.Scene()); boundPlayer = nullptr; worldOpen = playerOpen = false;
    ResetScope(); worldStep = {}; worldGuard.Reset({life.identity.scene,0,0}, Domain::World);
}
extern "C" void PlayerTemporal_ActorCreated(Actor* actor) {
    PlayerContact::Created(actor);
    if (actor->id != ACTOR_PLAYER) return;
    boundPlayer = reinterpret_cast<Player*>(actor);
    Check(life.CreatePlayer()); ResetScope();
}
extern "C" void PlayerTemporal_ActorDestroyed(Actor* actor) {
    if (actor == reinterpret_cast<Actor*>(boundPlayer)) rollRecovery={};
    if (actor == mountOffer) mountOffer = nullptr;
    if (actor == earlyTalkOffer.actor) earlyTalkOffer = {};
    StalfosTemporal_Destroyed(actor);
    PlayerContact::Destroyed(actor);
    if (actor != reinterpret_cast<Actor*>(boundPlayer)) return;
    Check(life.DestroyPlayer()); boundPlayer = nullptr; ResetScope();
}
extern "C" void PlayerTemporal_BeginFrame() {
    // Physical right bumper is an edge-triggered live A/B shortcut. Do not
    // consume or remap gameplay input, and do not toggle during mapper capture.
    static bool bumperHeld = false;
    if (!NativeSimTest_IsEnabled() && Ship::Context::GetRawInstance()) {
        auto deck = Ship::Context::GetRawInstance()->GetControlDeck();
        bool held = false;
        for (const auto& pad : deck->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(0))
            held |= SDL_GameControllerGetButton(pad.second, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
        if (held && !bumperHeld && !deck->GamepadGameInputBlocked()) {
            const int rate = CVarGetInteger(PLAYER_EXPERIMENTAL_HZ_CVAR,20);
            CVarSetInteger(PLAYER_EXPERIMENTAL_HZ_CVAR,rate == 20 ? 120 : 20);
            if (CVarGetInteger("gDeveloperTools.NativeStalfosArena",0))
                CVarSetInteger("gDeveloperTools.NativeStalfosHz",rate == 20 ? 120 : 0);
        }
        bumperHeld = held;
    }
    StalfosTemporal_BeginFrame();
    mountOffer = nullptr;
    earlyTalkOffer = {};
    hostFrameStart = GetPerfCounter();
    worldOpen = playerOpen = false; highWorld = false; poseBinding = nullptr; presentationPacket = nullptr; viewPrepared = false;
}
extern "C" uint64_t PlayerTemporal_HostFrameStart() { return hostFrameStart; }
extern "C" void PlayerTemporal_EndFrame() {
    if (worldOpen) { worldTime = worldStep.endTime; worldSteps = worldStep.worldStepId; }
    if (highWorld) {
        Check(!playerOpen && playerClock.EndWorld());
    } else if (playerOpen) { Check(Add(playerTime,{WorldStepQuanta},playerTime)); playerSteps = playerStep.playerStepId; }
    worldOpen = playerOpen = false;
    presentationPacket = nullptr;
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
        highRejection = "paused, transitioning, frozen or Player unavailable";
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
    } else if (std::strcmp(site,"collision.after_at") == 0 && worldOpen) {
        PlayerContact::Consume(play, life.identity, worldStep.startTime);
    } else if (std::strcmp(site,"actor.update.begin") == 0 && worldOpen && GET_PLAYER(play) == boundPlayer) {
        Check(!playerOpen && playerSteps != UINT64_MAX);
        uint32_t current = uint32_t(boundPlayer->currentShield) | (uint32_t(boundPlayer->currentBoots)<<8) |
                           (uint32_t(boundPlayer->heldItemAction & 0xff)<<16) | (uint32_t(boundPlayer->currentTunic)<<24);
        if (equipmentKnown && current != equipment) {
            ++equipmentGeneration; InvalidateScope();
        }
        equipment = current; equipmentKnown = true;
        unsigned selected = NativeSimTest_IsEnabled() ?
            (NativeSimTest_IsMeasuring() ? NativeSimTest_ConfigInt("player_hz",20) : 20) :
            CVarGetInteger(PLAYER_EXPERIMENTAL_HZ_CVAR,20);
        if (!ValidRate(selected)) selected = 20;
        if (!NativeSimTest_IsEnabled() && selected != requestedHz) {
            InvalidateScope(true); // apply at the shared boundary, retaining acquired edges
            highFallback = false; highPlay = nullptr; latchedRejection.clear();
            highRejection = selected == 20 ? "Original 20 Hz" : "";
        }
        requestedHz = selected;
        admittedHz = 20;
        if (requestedHz == 20) highRejection = "Original 20 Hz";
        if (requestedHz == 60 || requestedHz == 120) {
            const unsigned quanta = 120 / requestedHz;
            // A scene can install the inert Player_StartMode_Nothing actor:
            // it has no action or draw callback. Unrestricted gameplay selection
            // must not bypass that actor lifecycle or execute a null action.
            const char* rejection = suspended || !boundPlayer->actionFunc || !boundPlayer->actor.draw ?
                "Player update suspended or not initialized" :
                (PlayerTemporal_UnrestrictedPilot() ? nullptr :
                 Player_HighRateProfileRejection(play, boundPlayer, &play->state.input[0], quanta, true));
            const size_t reserve = 5 * (sizeof(PlayerPosePacket) + sizeof(Gfx) * PoseCommandCapacity);
            if (!rejection && reinterpret_cast<uintptr_t>(THGA_GetTail(&play->state.gfxCtx->polyOpa)) -
                reinterpret_cast<uintptr_t>(THGA_GetHead(&play->state.gfxCtx->polyOpa)) < reserve + 32768)
                rejection = "insufficient synchronous pose storage";
            highRejection = rejection ? rejection : "";
            if (rejection && highPlay && !highFallback) {
                highFallback = true; latchedRejection = rejection;
                InvalidateScope();
            }
            // A transient failure cancels only the prior transaction. Retry the
            // user's selected rate at the next shared boundary, with queued input.
            if (!rejection && PlayerTemporal_UnrestrictedPilot()) {
                highFallback = false; latchedRejection.clear();
            }
            if (!rejection && !highFallback) {
                admittedHz = requestedHz;
                intermediatePackets = static_cast<PlayerPosePacket*>(Graph_Alloc(play->state.gfxCtx,5*sizeof(PlayerPosePacket)));
                intermediateCommands = static_cast<Gfx*>(Graph_Alloc(play->state.gfxCtx,5*sizeof(Gfx)*PoseCommandCapacity));
                intermediatePacketIndex = 0;
                Check(playerClock.Reset(life.identity,worldTime,playerSteps,worldSteps));
                Check(playerClock.Request(requestedHz)); Check(playerClock.BeginWorld(true)); Check(playerClock.BeginPlayer());
                highWorld = true; highPlay = play;
            }
        }
        // Context intervals use shared simulation availability time. Elapsed
        // Player time separately counts only transactions that update Player.
        playerStep = highWorld ? playerClock.Player() : PlayerStepContext{
            SimulationRate::Hz20,WorldStepQuanta,worldTime,worldStep.endTime,playerSteps+1,life.identity};
        // BG horses offer mounts before Player updates. Legacy Player then
        // clears rideActor, unlike NPC offers produced after Player. Keep the
        // incoming offer for Player-only steps without changing the world slot.
        mountOffer = highWorld && !boundPlayer->actor.parent ? boundPlayer->rideActor : nullptr;
        mountOfferSide = boundPlayer->mountSide;
        // Soil and other early-category actors offer before Player, which clears
        // an unaccepted offer at the end of its world update. Later NPC producers
        // already leave their offers available to intermediate steps.
        earlyTalkOffer = {};
        if (highWorld && !(boundPlayer->actor.flags & ACTOR_FLAG_TALK) && boundPlayer->talkActor &&
            boundPlayer->talkActor->category < ACTORCAT_PLAYER) {
            earlyTalkOffer = {boundPlayer->talkActor, boundPlayer->talkActorDistance, boundPlayer->exchangeItemId};
        }
        playerOpen = true;
        PlayerContact::BeginStep();
        if (inputRequested) {
            InputEvent event;
            while (inputs.ConsumeForPlayer(playerStep,event, highWorld ? 0 : 4)) lastInput = event;
            inputRequested = false;
        }
    } else if (std::strcmp(site,"actor.update.end") == 0 && playerOpen) {
        Check(life.animation.Advance());
    }
}
extern "C" void PlayerTemporal_BindPresentation(PlayState* play, const void* prepared, int lod, void* binding) {
    if (!highWorld || !playerOpen || play != highPlay) { Check(false); return; }
    const auto* packet = static_cast<const PlayerPosePacket*>(prepared);
    Check(packet->playerIdentity == boundPlayer && packet->sceneIdentity == play);
    poseBinding = static_cast<Gfx*>(binding); poseLod = lod; poseObjectSegment = gSegments[6];
    presentationPacket = packet;
}
extern "C" unsigned PlayerTemporal_BeginPresentation() {
    if (!highWorld) return 0;
    if (!playerOpen || !highPlay || GET_PLAYER(highPlay) != boundPlayer) {
        Check(false); return 0;
    }
    projectionKey = highPlay->view.projectionPtr; viewingKey = highPlay->view.viewingPtr;
    CaptureControlView();
    PlayerContact::Capture(highPlay, worldStep);
    CommitHighPlayer();
    return requestedHz;
}
extern "C" unsigned PlayerTemporal_NextPlayerOffset() {
    if (!highWorld || highFallback) return WorldStepQuanta;
    return static_cast<unsigned>(playerClock.Now().quanta - playerClock.World().startTime.quanta);
}
extern "C" int PlayerTemporal_AdvanceIntermediate() {
    if (!highWorld || highFallback || playerClock.PlayerOpen() || PlayerTemporal_NextPlayerOffset() >= WorldStepQuanta)
        return 0;
    if (!boundPlayer || GET_PLAYER(highPlay) != boundPlayer ||
        !boundPlayer->actionFunc || !boundPlayer->actor.draw) {
        RevokeHigh("Player lifetime ended or update became unavailable");
        return 0;
    }
    Input input;
    const auto offset = PlayerTemporal_NextPlayerOffset();
    NativeSimTest_WaitPlayer(offset);
    if (!PadMgr_PollPlayer(&gPadMgr, NativeSimTest_TimeQ() + offset) || !PadMgr_GetPlayerSample(&gPadMgr,&input,false)) {
        RevokeHigh("unsupported input acquisition"); return 0;
    }
    const char* reason = PlayerTemporal_UnrestrictedPilot() ? nullptr :
        Player_HighRateProfileRejection(highPlay,boundPlayer,&input,120/requestedHz,false);
    if (reason) { RevokeHigh(reason); return 0; }
    if (intermediatePacketIndex >= 5) { RevokeHigh("pose packet capacity"); return 0; }
    PlayerAnimationQueue queue{};
    if (!PlayerAnimation_BeginQueue(highPlay,&queue)) { RevokeHigh("Player animation queue unavailable"); return 0; }
    Check(playerClock.BeginPlayer()); playerStep = playerClock.Player(); playerOpen = true;
    PlayerContact::BeginStep();
    Check(PadMgr_GetPlayerSample(&gPadMgr,&input,true));
    InputEvent event;
    while (inputs.ConsumeForPlayer(playerStep,event,0)) lastInput = event;
    inputRequested = false;
    NativeSimTest_PlayerSample("player_step.begin",highPlay);
    const bool borrowMountOffer = !boundPlayer->rideActor && !boundPlayer->actor.parent &&
        mountOffer && mountOffer->update;
    if (borrowMountOffer) {
        boundPlayer->rideActor = mountOffer;
        boundPlayer->mountSide = mountOfferSide;
    }
    const bool borrowTalkOffer = !boundPlayer->talkActor && !(boundPlayer->actor.flags & ACTOR_FLAG_TALK) &&
        earlyTalkOffer.actor && earlyTalkOffer.actor->update;
    const float savedTalkDistance = boundPlayer->talkActorDistance;
    const int savedExchangeItem = boundPlayer->exchangeItemId;
    if (borrowTalkOffer) {
        boundPlayer->talkActor = earlyTalkOffer.actor;
        boundPlayer->talkActorDistance = earlyTalkOffer.distance;
        boundPlayer->exchangeItemId = earlyTalkOffer.exchangeItem;
    }
    StalfosTemporal_Advance(highPlay);
    Player_AdvanceIntermediate(highPlay,boundPlayer,&input);
    if (borrowTalkOffer && !(boundPlayer->actor.flags & ACTOR_FLAG_TALK) &&
        boundPlayer->talkActor == earlyTalkOffer.actor) {
        boundPlayer->talkActor = nullptr;
        boundPlayer->talkActorDistance = savedTalkDistance;
        boundPlayer->exchangeItemId = savedExchangeItem;
    }
    if (borrowMountOffer && !boundPlayer->actor.parent &&
        !(boundPlayer->stateFlags1 & PLAYER_STATE1_ON_HORSE)) {
        // Leave the original empty offer slot for the next world producer.
        boundPlayer->rideActor = nullptr;
    }
    Check(life.animation.Advance());
    Check(PlayerAnimation_EndQueue(&queue));
    const bool cameraAdvanced = PlayerCamera_AdvanceControl(highPlay,playerStep.stepQuanta);
    Check(cameraAdvanced || PlayerTemporal_UnrestrictedPilot());
    // Other camera modes retain their ordinary world update; they do not veto Player Hz.
    if (poseBinding) {
    auto* packet = &intermediatePackets[intermediatePacketIndex];
    const auto previousSegment = gSegments[6]; gSegments[6] = poseObjectSegment;
    PlayerTemporal_PoseAdmission(boundPlayer,true);
    Player_AdvanceIntermediatePose(highPlay,boundPlayer,packet,poseLod);
    gSegments[6] = previousSegment;
    auto* commands = &intermediateCommands[intermediatePacketIndex++ * PoseCommandCapacity];
    uint32_t paint = 0;
    auto* end = static_cast<Gfx*>(NativeSimTest_Present("player",highPlay,packet,sizeof(*packet),commands,
        &paint,sizeof(paint),true,Player_DrawPosePresentation));
    gSPEndDisplayList(end);
    // Only this rendering indirection changes; previously committed packets and
    // command lists remain immutable in the current world transaction's arena.
    gSPDisplayList(poseBinding,commands);
    presentationPacket = packet;
    } // Draw-disabled/special draw paths keep their ordinary world pose ownership.
    CaptureControlView();
    NativeSimTest_PlayerSample("player_step.end",highPlay);
    CommitHighPlayer();
    return 1;
}
extern "C" const void* PlayerTemporal_PresentationPacket() { return presentationPacket; }
extern "C" void PlayerTemporal_PreparedView(void** projection, void** viewing,
                                            float projectionData[4][4], float viewingData[4][4]) {
    *projection = *viewing = nullptr;
    if (!viewPrepared) return;
    *projection = projectionKey; *viewing = viewingKey;
    std::memcpy(projectionData,preparedProjection.mf,sizeof(preparedProjection));
    std::memcpy(viewingData,preparedViewing.mf,sizeof(preparedViewing));
}
extern "C" void PlayerTemporal_ActionChanged(Player* player) {
    if (player != boundPlayer) return;
    rollRecovery={};
    doorNotificationWorld = 0;
    ++actionGeneration; life.attack.End(); angleRemainders = {}; aimRemainders = {}; angleFilters = {};
    analogStruggle = {};
}
extern "C" void PlayerTemporal_SetRollRecovery(Player* player, void* animation, float frame, float speed) {
    if (!PlayerTemporal_HighStepQuanta(player) || speed<=0) return;
    // Original roll exits at the first 20-Hz sample >=20. Its following idle
    // update continues the authored roll before run entry; the next world
    // update completes the run blend. Preserve that pose interval only.
    const float legacyAdvance=speed*R_UPDATE_RATE*0.5f;
    const float exitFrame=std::ceil(20.0f/legacyAdvance)*legacyAdvance;
    rollRecovery={static_cast<LinkAnimationHeader*>(animation),frame,speed,
                  exitFrame+legacyAdvance,exitFrame+2*legacyAdvance,1};
}
extern "C" void PlayerTemporal_ApplyRollRecovery(Player* player, PlayState* play) {
    const auto q=PlayerTemporal_HighStepQuanta(player);
    if (!q) { rollRecovery={}; return; }
    if (!rollRecovery.animation) return;
    rollRecovery.frame+=rollRecovery.speed*q*0.25f;
    rollRecovery.weight=std::clamp((rollRecovery.blendEnd-rollRecovery.frame)/
        (rollRecovery.blendEnd-rollRecovery.blendStart),0.0f,1.0f);
    if (rollRecovery.weight==0) { rollRecovery={}; return; }
    // Sample visual recovery without replaying roll movement, actions or SFX.
    LinkAnimation_LoadToMorph(play,&player->skelAnime,rollRecovery.animation,rollRecovery.frame);
    LinkAnimation_InterpJointMorph(play,&player->skelAnime,rollRecovery.weight);
}
extern "C" float PlayerTemporal_RollRecoveryWeight() { return rollRecovery.weight; }
extern "C" void PlayerTemporal_AttackStarted(Player* player) {
    if (player == boundPlayer) {
        Check(life.attack.Begin());
        PlayerTemporal_ResetPulse(player, PLAYER_PULSE_COMBO_POSE, 1);
        PlayerTemporal_ResetPulse(player, PLAYER_PULSE_BLUR, 1);
        PlayerTemporal_ResetPulse(player, PLAYER_PULSE_DUST, 1);
    }
}
extern "C" void PlayerTemporal_MeleeWindow(Player* player, int active) {
    if (player == boundPlayer) Check(life.attack.Window(active > 0));
}
extern "C" void PlayerTemporal_AnimationChanged(SkelAnime* animation) {
    if (boundPlayer && animation == &boundPlayer->skelAnime) Check(life.animation.Change());
    if (auto* state = AnimationState(animation)) {
        Check(state->events.Change()); Check(state->events.Advance());
        state->markerCount = 0;
        state->terminalHeld = false;
        state->legacyFrame = animation->curFrame;
        state->nextLegacyTime = playerStep.startTime.quanta + WorldStepQuanta;
        state->legacyComplete = false;
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
extern "C" int PlayerTemporal_WorldOpportunity(const Player* player) {
    return PlayerTemporal_HighStepQuanta(player) == 0 || playerStep.startTime.quanta == worldStep.startTime.quanta;
}
extern "C" int16_t PlayerTemporal_AnalogStruggle(const Player* player, int16_t previousAngle,
                                               int16_t angle, float magnitude) {
    if (!PlayerTemporal_HighStepQuanta(player)) return 0; // Canonical caller keeps its original expression.
    return analogStruggle.Sample(previousAngle, angle, magnitude, PlayerTemporal_WorldOpportunity(player));
}
extern "C" void PlayerTemporal_DoorAccepted(const Player* player) {
    if (player != boundPlayer) return;
    doorNotificationWorld = 0;
    const Actor* door = player->doorActor;
    if (player->doorTimer <= 0 || !door || (door->id != ACTOR_EN_DOOR && door->id != ACTOR_DOOR_SHUTTER) ||
        !PlayerTemporal_HighStepQuanta(player) || PlayerTemporal_WorldOpportunity(player)) return;
    // Intermediate acceptance is immediate. The next world update only tells
    // EnDoor/DoorShutter to open; their ten world20 lock decrements start later.
    doorNotificationWorld = worldStep.worldStepId + 1;
}
extern "C" int PlayerTemporal_DoorCountdownOpportunity(const Player* player) {
    if (!PlayerTemporal_WorldOpportunity(player)) return false;
    if (player == boundPlayer && doorNotificationWorld) {
        if (worldStep.worldStepId == doorNotificationWorld) return false;
        if (worldStep.worldStepId > doorNotificationWorld) doorNotificationWorld = 0;
    }
    // InvalidateScope deliberately retains this fence through rate fallback.
    // With no intermediate acceptance, canonical20 retains every old decrement.
    return true;
}
extern "C" int PlayerTemporal_TakeBurnFootstep(const Player* player) {
    const bool footstep = (player->stateFlags2 & PLAYER_STATE2_FOOTSTEP) != 0;
    if (!PlayerTemporal_HighStepQuanta(player)) {
        // Preserve a pending event when switching back to canonical at a world
        // boundary, but never carry it into another burning episode.
        const bool result = footstep || (player->bodyIsBurning && pendingBurnFootstep);
        pendingBurnFootstep = false;
        return result;
    }
    pendingBurnFootstep = player->bodyIsBurning && (pendingBurnFootstep || footstep);
    if (!PlayerTemporal_WorldOpportunity(player)) return false;
    const bool result = pendingBurnFootstep;
    pendingBurnFootstep = false;
    return result;
}
extern "C" uint16_t PlayerTemporal_WorldInputButtons() {
    uint16_t buttons = BTN_CUP | BTN_START | BTN_L;
    if (highPlay && boundPlayer && GET_PLAYER(highPlay) == boundPlayer) {
        // Message choices and the horse's carrot consumer run at world20.
        // Retain their edges in PadMgr until the next world opportunity; do
        // not consume them on an intermediate Player-only step.
        if (highPlay->msgCtx.msgMode != MSGMODE_NONE || (boundPlayer->actor.flags & ACTOR_FLAG_TALK))
            return 0xffff;
        if (boundPlayer->stateFlags1 & PLAYER_STATE1_ON_HORSE) buttons |= BTN_A;
    }
    return buttons;
}
extern "C" int PlayerTemporal_LegacyPulse(Player* player, PlayerLegacyPulse source) {
    if (!PlayerTemporal_HighStepQuanta(player)) return 1;
    if (source < 0 || source >= PLAYER_PULSE_COUNT) { Check(false); return 0; }
    return legacyPulses[source].Consume(playerStep.startTime, playerStep.playerStepId);
}
extern "C" void PlayerTemporal_ResetPulse(Player* player, PlayerLegacyPulse source, int immediate) {
    if (!PlayerTemporal_HighStepQuanta(player)) return;
    if (source < 0 || source >= PLAYER_PULSE_COUNT) { Check(false); return; }
    Check(legacyPulses[source].Reset(playerStep.startTime, immediate != 0));
}
extern "C" void PlayerTemporal_HoverVelocityReset(const Player* player) {
    if (!PlayerTemporal_HighStepQuanta(player)) return;
    hoverReset = {life.identity, playerStep.endTime.quanta, player->actor.velocity.y, true};
}
extern "C" int PlayerTemporal_AdvanceMotion(Player* player, float pushX, float pushZ) {
    if (!PlayerTemporal_HighStepQuanta(player)) return 0;
    auto& actor = player->actor;
    PlayerMotion state{{actor.world.pos.x, actor.world.pos.y, actor.world.pos.z},
        {Math_SinS(actor.world.rot.y) * actor.speedXZ + pushX, actor.velocity.y,
         Math_CosS(actor.world.rot.y) * actor.speedXZ + pushZ}, actor.gravity, actor.minVelocityY};
    ApplyHoverMotionConstraint(player, playerStep.startTime.quanta, state);
    hoverReset = {}; // One motion opportunity; skipped/overridden resets never persist.
    const auto& correction = actor.colChkInfo.displacement;
    if (!AdvancePlayerMotion(state, playerStep, {correction.x, correction.y, correction.z})) {
        if (!PlayerTemporal_UnrestrictedPilot()) { Check(false); return 0; }
        // Outside the qualified affine segment (e.g. terminal-velocity crossing),
        // keep displacement at Player cadence instead of falling through to a
        // full 50-ms Actor_UpdatePos on every substep. Canonical20 is unchanged.
        const float fraction = float(playerStep.stepQuanta) / 6.0f;
        state.velocity[1] = std::max(state.terminalVelocity, state.velocity[1] + state.gravity * fraction);
        const float corrections[3] = {correction.x,correction.y,correction.z};
        for (unsigned axis=0;axis<3;++axis) state.position[axis] +=
            state.velocity[axis] * (1.5f * fraction) +
            (CommonBoundary(playerStep.startTime) ? corrections[axis] : 0.0f);
    }
    actor.velocity = {state.velocity[0], state.velocity[1], state.velocity[2]};
    actor.world.pos = {state.position[0], state.position[1], state.position[2]};
    return 1;
}
extern "C" int PlayerTemporal_PredictMotion(const Player* player, unsigned quanta, int worldBoundary, float position[3]) {
    if (!player || !position || (quanta != 1 && quanta != 2)) return 0;
    const auto& actor = player->actor;
    PlayerMotion state{{actor.world.pos.x, actor.world.pos.y, actor.world.pos.z},
        {Math_SinS(player->yaw) * player->linearVelocity, actor.velocity.y,
         Math_CosS(player->yaw) * player->linearVelocity}, actor.gravity, actor.minVelocityY};
    // Admission predicts the next scheduler interval before opening it.
    const uint64_t nextTime = worldBoundary ? worldStep.startTime.quanta : playerClock.Now().quanta;
    ApplyHoverMotionConstraint(player, nextTime, state); // Read-only: execution consumes the reset.
    const uint64_t start = worldBoundary ? 0 : quanta;
    const PlayerStepContext step{quanta == 1 ? SimulationRate::Hz120 : SimulationRate::Hz60,
        quanta, {start}, {start + quanta}, 1, {}};
    const auto& correction = actor.colChkInfo.displacement;
    if (!AdvancePlayerMotion(state, step, {correction.x, correction.y, correction.z})) return 0;
    for (unsigned i = 0; i < 3; ++i) position[i] = state.position[i];
    return 1;
}
extern "C" int PlayerTemporal_StepAngle(Player* player, int16_t* angle, int16_t target, int16_t legacyStep) {
    return PlayerTemporal_StepScratchAngle(player, angle, target, legacyStep, 0);
}
extern "C" int16_t PlayerTemporal_AimIncrement(Player* player, int16_t* angle, int16_t legacyIncrement) {
    const unsigned quanta = PlayerTemporal_HighStepQuanta(player);
    if (!quanta) return legacyIncrement;
    const unsigned axis = angle == &player->actor.focus.rot.x ? 0 : 1;
    auto& remainder = aimRemainders[axis];
    if (!remainder.known || remainder.previous != *angle) remainder.fraction.Reset();
    int64_t delta = 0;
    Check(remainder.fraction.Advance(legacyIncrement, quanta, delta));
    remainder.previous = static_cast<int16_t>(WrapAngle(static_cast<uint16_t>(*angle), delta));
    remainder.known = true;
    return static_cast<int16_t>(delta);
}
extern "C" int PlayerTemporal_StepScratchAngle(Player* player, int16_t* angle, int16_t target,
                                               int16_t legacyStep, unsigned scratchOwner) {
    const unsigned quanta = PlayerTemporal_HighStepQuanta(player);
    if (!quanta) return 0; // Canonical callers must retain Math_ScaledStepToS.
    const unsigned i = AngleOwner(player, angle, scratchOwner);
    if (i < angleRemainders.size()) {
        auto& remainder = angleRemainders[i];
        if (!remainder.known || remainder.previous != *angle) remainder.fraction.Reset();
        const int32_t canonicalCap = static_cast<int32_t>(legacyStep * (R_UPDATE_RATE * 0.5f));
        bool reached = false;
        Check(AdvancePlayerAngle(*angle, target, canonicalCap, quanta, remainder.fraction, reached));
        remainder.known = true; remainder.previous = *angle;
        return reached;
    }
    Check(false); return 0;
}
extern "C" void PlayerTemporal_SmoothAngle(Player* player, int16_t* angle, int16_t target,
                                            float gain, float minimum, float maximum, unsigned scratchOwner) {
    const unsigned quanta = PlayerTemporal_HighStepQuanta(player);
    if (!quanta) { Check(false); return; }
    const unsigned i = AngleOwner(player, angle, scratchOwner);
    if (i >= angleFilters.size()) { Check(false); return; }
    Check(SmoothPlayerAngle(*angle, target, gain, minimum, maximum, quanta, angleFilters[i]));
}
extern "C" unsigned PlayerTemporal_HighAnimationQuanta(const SkelAnime* animation) {
    return AnimationState(animation) ? PlayerTemporal_HighStepQuanta(boundPlayer) : 0;
}
extern "C" void PlayerTemporal_AnimationAdvanced(SkelAnime* animation, float previousFrame) {
    auto* state = AnimationState(animation);
    if (!state) return;
    Check(state->events.Advance());
    state->from = previousFrame; state->to = animation->curFrame;
    if (PlayerTemporal_HighAnimationQuanta(animation)) {
        if (animation->mode == ANIMMODE_ONCE && playerStep.startTime.quanta >= state->nextLegacyTime) {
            state->nextLegacyTime = playerStep.startTime.quanta + WorldStepQuanta;
            if (state->legacyFrame == animation->endFrame) state->legacyComplete = true;
            else {
                state->legacyFrame += animation->playSpeed * 1.5f;
                if ((state->legacyFrame - animation->endFrame) * animation->playSpeed > 0)
                    state->legacyFrame = animation->endFrame;
            }
        }
        if (animation->curFrame != animation->endFrame) {
            state->terminalHeld = false;
        } else if (!state->terminalHeld) {
            state->terminalHeld = true;
            state->terminalStart = playerStep.startTime.quanta;
        }
    }
    if (animation->playSpeed > 0 && state->to < state->from) state->to += animation->animLength;
    if (animation->playSpeed < 0 && state->to > state->from) state->to -= animation->animLength;
}
extern "C" int PlayerTemporal_AnimationEndHeld(SkelAnime* animation) {
    if (!PlayerTemporal_HighAnimationQuanta(animation)) return 1;
    const auto* state = AnimationState(animation);
    return state->terminalHeld && playerStep.startTime.quanta - state->terminalStart >= 6;
}
extern "C" int PlayerTemporal_AnimationLegacyComplete(SkelAnime* animation) {
    return !PlayerTemporal_HighAnimationQuanta(animation) || AnimationState(animation)->legacyComplete;
}
extern "C" int PlayerTemporal_AnimationMarker(SkelAnime* animation, float marker) {
    auto* state = AnimationState(animation);
    if (!state || !PlayerTemporal_HighAnimationQuanta(animation)) return 0;
    for (float value : {state->from, state->to, marker, animation->animLength}) {
        if (!std::isfinite(value) || std::fabs(double(value) * 65536.0) > double(PhaseLimit)) {
            Check(false); return 0;
        }
    }
    const auto q16 = [](float frame) { return static_cast<int64_t>(std::llround(double(frame) * 65536.0)); };
    // Legacy shared SFX lists can name frames beyond the current animation.
    // Those markers never fire; they are not malformed simulation intervals.
    if (marker < 0 || marker >= animation->animLength) return 0;
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
    if (admissionKnown && poseAdmitted && !admitted && !PlayerTemporal_UnrestrictedPilot()) {
        InvalidateScope();
    }
    admissionKnown = true; poseAdmitted = admitted != 0;
}
extern "C" void PlayerTemporal_InputSample(uint64_t seq, uint64_t num, uint64_t den,
                                            unsigned port, uint32_t held, uint16_t press, uint16_t release) {
    if (!life.playerAlive || suspended || port >= 4) return;
    const SimTime available = highWorld ? playerClock.Now() : worldTime;
    Check(inputs.Queue({life.identity,seq,num,den,available,held,press,release,static_cast<uint8_t>(port)}));
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
PlayerTemporalObservation PlayerTemporal_Observe() {
    return {okay,highWorld && !highFallback,playerStep.startTime.quanta,playerStep.endTime.quanta,
        playerSteps,poses,life.animation.generation,inputs.consumingPlayerStep,lastInput.sequence,
        lastInput.pressed,lastInput.released};
}
extern "C" PlayerRateStatus PlayerTemporal_RateStatus() {
    unsigned requested = NativeSimTest_IsEnabled() ? requestedHz : CVarGetInteger(PLAYER_EXPERIMENTAL_HZ_CVAR,20);
    if (!ValidRate(requested)) requested = 20;
    // Persistent admission changes only at scheduler/lifetime boundaries, never
    // when BeginFrame clears the transient current-transaction highWorld flag.
    return {requested, admittedHz, admittedHz != 20, highFallback,
            highRejection.c_str(), latchedRejection.c_str()};
}
nlohmann::json PlayerTemporal_Inspect() {
    nlohmann::json result = {{"okay",okay},{"requested_player_hz",requestedHz},
        {"effective_player_hz",highWorld && !highFallback ? requestedHz : 20},
        {"world_hz",capability.WorldRate()},{"player_high_rate_admitted",highWorld && !highFallback},
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
        {"contact_queue_count",PlayerContact::Inspect()["pending"]},{"contact_bridge_active",highWorld},
        {"contact_bridge",PlayerContact::Inspect()}};
    if (requestedHz != 20) {
        result["high_rate_rejection"] = highRejection;
        result["high_rate_fallback_latched"] = highFallback;
    }
    if (!okay) result["first_failure_line"] = firstFailureLine;
    return result;
}
