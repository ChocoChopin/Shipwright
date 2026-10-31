#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct Actor;
struct PlayState;
struct SkelAnime;
/* One opt-in ordinary Stalfos. World action callbacks remain world-owned. */
void StalfosTemporal_Reset(void);
void StalfosTemporal_BeginFrame(void);
void StalfosTemporal_ArenaWorld(struct PlayState* play);
int StalfosTemporal_ArenaRestart(struct PlayState* play);
int StalfosTemporal_GroundArenaActor(const struct Actor* actor);
void StalfosTemporal_Destroyed(struct Actor* actor);
void StalfosTemporal_BeginUpdate(struct Actor* actor, struct PlayState* play);
void StalfosTemporal_EndUpdate(struct Actor* actor);
void StalfosTemporal_RiseStep(struct Actor* actor);
int StalfosTemporal_RiseAdvanced(void);
void StalfosTemporal_BreakLimb(struct Actor* actor, int limb, int visible);
float StalfosTemporal_Duration(struct Actor* actor);
int StalfosTemporal_Animation(struct Actor* actor, struct SkelAnime* animation);
void StalfosTemporal_Move(struct Actor* actor);
int StalfosTemporal_IsActor(const struct Actor* actor);
int StalfosTemporal_Managed(const struct Actor* actor);
int StalfosTemporal_SkipWorldPair(const void* attack, const void* defense);
int StalfosTemporal_IsIntermediate(void);
int StalfosTemporal_IsContact(void);
void StalfosTemporal_ActionChanged(struct Actor* actor);
int StalfosTemporal_Draw(struct PlayState* play, struct Actor* actor);
void StalfosTemporal_Advance(struct PlayState* play);
void StalfosTemporal_Contacts(struct PlayState* play, uint64_t attackEpoch, uint64_t opportunity);
typedef struct {
    uint64_t worlds, steps, poses, animations, movingSteps, attackSamples, contacts, blocks;
    uint64_t horizontalSteps, culledPoses, arenaRounds;
    uint64_t breakPoseRefreshes, eulerCorrections, deathEulerCorrections;
    uint64_t arenaDestroyed, arenaAttempts, arenaNoGround, arenaSpawnFailed, arenaBlocked;
    float arenaFloor[8], arenaFloorDelta[8];
    int16_t arenaNormal[8], arenaFloorType[8], arenaFloorProperty[8];
    uint64_t playerContacts, decisions, invalid, renderCommands;
    float maxStep;
} StalfosTemporalStats;
StalfosTemporalStats StalfosTemporal_Stats(void);
/* Explicit duration for normal (non-Link) animation. No global rate mutation. */
int SkelAnime_UpdateDuration(struct SkelAnime* animation, float duration);
int FrameInterpolation_SetRecording(int enabled);
/* Actor-local bounded continuous work, never its tactical action callback. */
void EnTest_AdvanceTemporal(struct Actor* actor, struct PlayState* play, float previousFrame);
#ifdef __cplusplus
}
#endif
