#ifndef NATIVE_SIMULATION_TEST_H
#define NATIVE_SIMULATION_TEST_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct PlayState;
struct Actor;
struct Player;
struct PadMgr;
void NativeSimTest_Init(int argc, char** argv);
void NativeSimTest_Configure(void);
int NativeSimTest_IsEnabled(void);
int NativeSimTest_IsMeasuring(void);
int NativeSimTest_ConfigInt(const char* key, int fallback);
uint64_t NativeSimTest_TimeQ(void);
uint32_t NativeSimTest_Seed(void);
uint32_t NativeSimTest_AudioClock(void);
void NativeSimTest_AudioBlock(int samples);
void NativeSimTest_Rng(const char* stream, const char* site, uint32_t state);
void NativeSimTest_Event(const char* kind, const char* site, uint32_t value);
void NativeSimTest_BootSave(void);
int NativeSimTest_ReplayPad(struct PadMgr* padMgr);
void NativeSimTest_BeginFrame(void);
void NativeSimTest_EndFrame(void);
void NativeSimTest_Phase(const char* phase, struct PlayState* play);
void NativeSimTest_ActorSpawn(struct Actor* actor);
void NativeSimTest_ActorDestroy(struct Actor* actor);
void NativeSimTest_ActorFree(struct Actor* actor);
void NativeSimTest_SceneInit(void);
void NativeSimTest_ActorScope(struct Actor* actor);
const char* NativeSimTest_PlayerActionName(struct Player* player);
#ifdef __cplusplus
}
#endif
#endif
