#pragma once
#include <stdint.h>
#define PLAYER_EXPERIMENTAL_HZ_CVAR "gDeveloperTools.NativePlayerHz"
#ifdef __cplusplus
extern "C" {
#endif
struct Actor;
struct Player;
struct PlayState;
struct SkelAnime;
struct PadMgr;
void PlayerTemporal_SceneInit(void);
void PlayerTemporal_ActorCreated(struct Actor* actor);
void PlayerTemporal_ActorDestroyed(struct Actor* actor);
void PlayerTemporal_BeginFrame(void);
void PlayerTemporal_EndFrame(void);
typedef struct {
    unsigned requested, effective;
    int admitted, fallbackLatched;
    const char* rejection; /* Most recent world-boundary admission check. */
    const char* latchReason; /* Original reason retained until rearmed. */
} PlayerRateStatus;
PlayerRateStatus PlayerTemporal_RateStatus(void);
int PlayerTemporal_UnrestrictedPilot(void);
void PlayerTemporal_PlayBoundary(struct PlayState* play);
void PlayerTemporal_Sample(const char* site, struct PlayState* play);
void PlayerTemporal_ActionChanged(struct Player* player);
void PlayerTemporal_SetRollRecovery(struct Player* player, void* animation, float frame, float speed);
void PlayerTemporal_ApplyRollRecovery(struct Player* player, struct PlayState* play);
float PlayerTemporal_RollRecoveryWeight(void);
void PlayerTemporal_ShieldPresentation(struct Player* player, void* packet, const void* modelMatrix);
uint64_t PlayerTemporal_ShieldPresentationSamples(void);
void PlayerTemporal_AttackStarted(struct Player* player);
void PlayerTemporal_MeleeWindow(struct Player* player, int active);
void PlayerTemporal_AnimationChanged(struct SkelAnime* animation);
void PlayerTemporal_PoseAdmission(struct Player* player, int admitted);
void PlayerTemporal_InputSample(uint64_t sequence, uint64_t numerator, uint64_t denominator,
                               unsigned port, uint32_t held, uint16_t pressed, uint16_t released);
void PlayerTemporal_LiveInput(struct PadMgr* padMgr);
void PlayerTemporal_InputConsumed(void);
/* Nonzero only while the scheduler owns an admitted noncanonical Player step.
 * Animation returns authored-frame gain q/4 (legacy gain is 1.5 at world20).
 * Callers retain their original expressions when these queries return zero. */
unsigned PlayerTemporal_HighStepQuanta(const struct Player* player);
int PlayerTemporal_WorldOpportunity(const struct Player* player);
/* Analog escape retains integer world-sample scoring; button edges stay Player-rate. */
int16_t PlayerTemporal_AnalogStruggle(const struct Player* player, int16_t previousAngle,
                                    int16_t angle, float magnitude);
/* Locked-door waits count world20 opportunities after the actor's notification
 * update. Call acceptance after action setup; the fence belongs to that action. */
void PlayerTemporal_DoorAccepted(const struct Player* player);
int PlayerTemporal_DoorCountdownOpportunity(const struct Player* player);
// Sample each Player update; consume the transient extinguish event at world20.
int PlayerTemporal_TakeBurnFootstep(const struct Player* player);
/* UI edges remain pending for the message owner; movement input stays Player-rate. */
uint16_t PlayerTemporal_WorldInputButtons(void);
/* Bounded control camera, separate from world interface/quake/environment work. */
const char* PlayerCamera_ProfileRejection(struct PlayState* play);
int PlayerCamera_AdvanceControl(struct PlayState* play, unsigned quanta);
void PlayerCamera_ResetPolicy(void);
void PlayerTemporal_ContractFailure(void);
/* Internal gated scheduler. Presentation service advances only due fixed
 * intervals; rendering itself never calls the authority operation. */
void PlayerTemporal_BindPresentation(struct PlayState* play, const void* packet, int lod, void* binding);
unsigned PlayerTemporal_BeginPresentation(void);
uint64_t PlayerTemporal_HostFrameStart(void);
unsigned PlayerTemporal_NextPlayerOffset(void);
int PlayerTemporal_AdvanceIntermediate(void);
/* Latest pose bound for this synchronous world transaction, or NULL. */
const void* PlayerTemporal_PresentationPacket(void);
void PlayerTemporal_PreparedView(void** projection, void** viewing, float projectionData[4][4], float viewingData[4][4]);
/* Record only the actual hover branch's reset, valid for its next Player step. */
void PlayerTemporal_HoverVelocityReset(const struct Player* player);
int PlayerTemporal_AdvanceMotion(struct Player* player, float pushX, float pushZ);
int PlayerTemporal_PredictMotion(const struct Player* player, unsigned quanta, int worldBoundary, float position[3]);
int PlayerTemporal_StepAngle(struct Player* player, int16_t* angle, int16_t target, int16_t legacyStep);
/* Aim input is an angular increment per legacy50-ms update, not a target. */
int16_t PlayerTemporal_AimIncrement(struct Player* player, int16_t* angle, int16_t legacyIncrement);
/* Scratch owners are explicit semantic slots, never retained stack pointers. */
int PlayerTemporal_StepScratchAngle(struct Player* player, int16_t* angle, int16_t target,
                                   int16_t legacyStep, unsigned owner);
void PlayerTemporal_SmoothAngle(struct Player* player, int16_t* angle, int16_t target,
                                float gain, float minimum, float maximum, unsigned scratchOwner);
typedef enum {
    PLAYER_PULSE_COMBO_WINDOW,
    PLAYER_PULSE_TARGET_TIMER,
    PLAYER_PULSE_COMBO_POSE,
    PLAYER_PULSE_BLUR,
    PLAYER_PULSE_DUST,
    PLAYER_PULSE_COUNT
} PlayerLegacyPulse;
int PlayerTemporal_LegacyPulse(struct Player* player, PlayerLegacyPulse source);
void PlayerTemporal_ResetPulse(struct Player* player, PlayerLegacyPulse source, int immediate);
unsigned PlayerTemporal_HighAnimationQuanta(const struct SkelAnime* animation);
void PlayerTemporal_AnimationAdvanced(struct SkelAnime* animation, float previousFrame);
/* Melee completion owns one legacy (50 ms) terminal-pose interval, not one substep. */
int PlayerTemporal_AnimationEndHeld(struct SkelAnime* animation);
/* Optional original roll braking; presentation handoff never changes colliders. */
int PlayerTemporal_SmoothRollRecovery(void);
void PlayerTemporal_BeginRollBrake(struct Player* player);
void PlayerTemporal_ApplyRollBrake(struct Player* player);
void PlayerTemporal_RequestPoseHandoff(void);
/* Climbing cycles retain the legacy completion opportunity while their pose
 * and root motion advance at Player cadence. Resets with animation selection. */
int PlayerTemporal_AnimationLegacyComplete(struct SkelAnime* animation);
int PlayerTemporal_AnimationMarker(struct SkelAnime* animation, float marker);
int PlayerTemporal_ConsumeAnimationMarker(struct SkelAnime* animation, float marker, uint64_t eventId);
#ifdef __cplusplus
}
#include <nlohmann/json.hpp>
// Read-only, allocation-free test observation. Same owners as Inspect().
struct PlayerTemporalObservation {
    bool okay, high;
    uint64_t start, end, step, pose, animation, consumingStep, sequence;
    unsigned pressed, released;
};
PlayerTemporalObservation PlayerTemporal_Observe();
nlohmann::json PlayerTemporal_Inspect();
#endif
