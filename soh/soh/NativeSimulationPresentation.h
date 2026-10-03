#ifndef NATIVE_SIMULATION_PRESENTATION_H
#define NATIVE_SIMULATION_PRESENTATION_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct PlayState;
/* Synchronous CPU emission only. Output and observation storage belong to the
 * caller. The packet and all referenced resources remain valid for this call. */
typedef void* (*NativeSimPresentationHelper)(const void* packet, void* output, void* paint);
void* NativeSimTest_Present(const char* helper, struct PlayState* play, const void* packet,
                          void* output, void* paint, size_t paintSize, int visible,
                          NativeSimPresentationHelper emit);
int Interface_IsCountdownProfileAdmitted(struct PlayState* play);
int Message_IsPlainTextProfileAdmitted(struct PlayState* play);
size_t Message_CopyPresentationStatics(void* output);
#ifdef __cplusplus
}
#endif
#endif
