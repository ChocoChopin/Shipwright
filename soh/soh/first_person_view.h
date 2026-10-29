#pragma once
#include "include/z64math.h"
#ifdef __cplusplus
#include <unordered_map>
// Preserve the interpolated view-space animation under a responsive camera.
void FrameInterpolation_RebaseFirstPerson(std::unordered_map<Mtx*, MtxF>& replacements,
                                         Mtx* viewingKey, const MtxF& currentView, float alpha);
struct FirstPersonViewStats { unsigned frames = 0, blendedFrames = 0, matrices = 0, attachmentMatrices = 0, lastAttachmentMatrices = 0; float maxViewError = 0; };
FirstPersonViewStats FrameInterpolation_GetFirstPersonStats();
extern "C" {
#endif
void FrameInterpolation_FirstPersonScope(int enabled, const void* owner, int profile);
void FrameInterpolation_FirstPersonAttachmentScope(int enabled, const void* parent, const void* attachment);
void FrameInterpolation_RecordFirstPersonView(Mtx* key, const MtxF* view);
unsigned FrameInterpolation_TestFirstPersonView(void);
#ifdef __cplusplus
}
#endif
