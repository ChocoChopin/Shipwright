#ifndef LETTERBOX_H
#define LETTERBOX_H

#include <libultraship/libultra.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LETTERBOX_FILL_MIDDLE,
    LETTERBOX_FILL_TOP,
    LETTERBOX_FILL_BOTTOM
} LetterboxFill;

// Presentation-only command patches. Pointers live for one synchronous graph
// transaction and are discarded before its display-list arena can be reused.
void Letterbox_BeginFrame(s32 size);
s32 Letterbox_ShouldDraw(s32 size);
void Letterbox_Fill(Gfx** displayList, s32 left, s32 right, s32 height, s32 size, LetterboxFill kind);
void Letterbox_RecordScissor(Gfx* command, s32 left, s32 top, s32 right, s32 bottom, s32 size);
void Letterbox_Interpolate(f32 fraction);
void Letterbox_EndFrame(void);

#ifdef __cplusplus
}
#endif

#endif
