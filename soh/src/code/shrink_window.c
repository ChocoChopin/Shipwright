#include "global.h"
#include "letterbox.h"

// Six commands in the ordinary view (four wide fills and two scissors). Extra
// views may add scissors; if the bound is exceeded, retain the original frame.
typedef struct {
    Gfx* command;
    Gfx original[2];
    s32 left, top, right, bottom;
    s32 kind; // -1: scissor; otherwise LetterboxFill
} LetterboxCommand;

static LetterboxCommand sLetterboxCommands[16];
static s32 sLetterboxCommandCount;
static s32 sLetterboxPreviousSize, sLetterboxDrawSize;
static s32 sLetterboxHasPrevious, sLetterboxRecording;

void Letterbox_BeginFrame(s32 size) {
    sLetterboxPreviousSize = sLetterboxHasPrevious ? sLetterboxDrawSize : size;
    sLetterboxDrawSize = size;
    sLetterboxHasPrevious = true;
    sLetterboxCommandCount = 0;
    sLetterboxRecording = true;
}

s32 Letterbox_ShouldDraw(s32 size) {
    return size > 0 || (sLetterboxRecording && size == sLetterboxDrawSize && sLetterboxPreviousSize > 0);
}

static void Letterbox_WriteFill(Gfx* command, s32 left, s32 right, s32 height, s32 size, s32 kind) {
    s32 top = kind == LETTERBOX_FILL_MIDDLE ? size : (kind == LETTERBOX_FILL_BOTTOM ? height - size : 0);
    s32 bottom = kind == LETTERBOX_FILL_MIDDLE ? height - size - 1 :
                 (kind == LETTERBOX_FILL_TOP ? size - 1 : height - 1);
    if (kind != LETTERBOX_FILL_MIDDLE && size == 0) {
        // The last shrinking interval still needs bars on its early replays.
        // Its canonical endpoint draws nothing, never a negative-height fill.
        gSPNoOp(command++);
        gSPNoOp(command++);
    } else {
        gDPFillWideRectangle(command++, left, top, right, bottom);
    }
}

static void Letterbox_Record(Gfx* command, s32 left, s32 top, s32 right, s32 bottom, s32 size, s32 kind) {
    LetterboxCommand* entry;
    if (!sLetterboxRecording || size != sLetterboxDrawSize || sLetterboxPreviousSize == sLetterboxDrawSize) {
        return;
    }
    if (sLetterboxCommandCount == ARRAY_COUNT(sLetterboxCommands)) {
        sLetterboxCommandCount = 0;
        sLetterboxRecording = false;
        return;
    }
    entry = &sLetterboxCommands[sLetterboxCommandCount++];
    entry->command = command;
    entry->original[0] = command[0];
    if (kind != -1) entry->original[1] = command[1];
    entry->left = left;
    entry->top = top;
    entry->right = right;
    entry->bottom = bottom;
    entry->kind = kind;
}

void Letterbox_Fill(Gfx** displayList, s32 left, s32 right, s32 height, s32 size, LetterboxFill kind) {
    Gfx* command = *displayList;
    Letterbox_WriteFill(command, left, right, height, size, kind);
    *displayList += 2; // gDPFillWideRectangle is a two-command instruction.
    Letterbox_Record(command, left, 0, right, height, size, kind);
}

void Letterbox_RecordScissor(Gfx* command, s32 left, s32 top, s32 right, s32 bottom, s32 size) {
    Letterbox_Record(command, left, top, right, bottom, size, -1);
}

void Letterbox_Interpolate(f32 fraction) {
    s32 i;
    s32 size;
    fraction = CLAMP(fraction, 0.0f, 1.0f);
    size = (s32)(sLetterboxPreviousSize + (sLetterboxDrawSize - sLetterboxPreviousSize) * fraction + 0.5f);
    for (i = 0; i < sLetterboxCommandCount; ++i) {
        LetterboxCommand* entry = &sLetterboxCommands[i];
        if (fraction == 1.0f) {
            entry->command[0] = entry->original[0];
            if (entry->kind != -1) entry->command[1] = entry->original[1];
        } else if (entry->kind == -1) {
            s32 inset = CLAMP(size, 0, SCREEN_HEIGHT / 2);
            gDPSetScissor(entry->command, G_SC_NON_INTERLACE, entry->left, entry->top + inset,
                         entry->right, entry->bottom - inset);
        } else {
            Letterbox_WriteFill(entry->command, entry->left, entry->right, entry->bottom, size, entry->kind);
        }
    }
}

void Letterbox_EndFrame(void) {
    Letterbox_Interpolate(1.0f);
    sLetterboxCommandCount = 0;
    sLetterboxRecording = false;
}

s32 D_8012CED0 = 0;

s32 sShrinkWindowVal = 0;
s32 sShrinkWindowCurrentVal = 0;

void Letterbox_SetSizeTarget(s32 value) {
    if (CVarGetInteger(CVAR_ENHANCEMENT("DisableBlackBars"), 0)) {
        sShrinkWindowVal = 0;
        return;
    }
    if (HREG(80) == 0x13 && HREG(81) == 1) {
        osSyncPrintf("shrink_window_setval(%d)\n", value);
    }
    sShrinkWindowVal = value;
}

u32 ShrinkWindow_GetVal(void) {
    return sShrinkWindowVal;
}

void ShrinkWindow_SetCurrentVal(s32 currentVal) {
    // Explicit camera/cutscene assignments are cuts, not animated approaches.
    sLetterboxHasPrevious = false;
    if (CVarGetInteger(CVAR_ENHANCEMENT("DisableBlackBars"), 0)) {
        sShrinkWindowCurrentVal = 0;
        return;
    }
    if (HREG(80) == 0x13 && HREG(81) == 1) {
        osSyncPrintf("shrink_window_setnowval(%d)\n", currentVal);
    }
    sShrinkWindowCurrentVal = currentVal;
}

u32 ShrinkWindow_GetCurrentVal(void) {
    return sShrinkWindowCurrentVal;
}

void ShrinkWindow_Init(void) {
    sLetterboxHasPrevious = false;
    sLetterboxCommandCount = 0;
    sLetterboxRecording = false;
    if (HREG(80) == 0x13 && HREG(81) == 1) {
        osSyncPrintf("shrink_window_init()\n");
    }
    D_8012CED0 = 0;
    sShrinkWindowVal = 0;
    sShrinkWindowCurrentVal = 0;
}

void ShrinkWindow_Destroy(void) {
    sLetterboxHasPrevious = false;
    sLetterboxCommandCount = 0;
    sLetterboxRecording = false;
    if (HREG(80) == 0x13 && HREG(81) == 1) {
        osSyncPrintf("shrink_window_cleanup()\n");
    }
    sShrinkWindowCurrentVal = 0;
}

void ShrinkWindow_Update(s32 updateRate) {
    s32 off;

    if (updateRate == 3) {
        off = 10;
    } else {
        off = 30 / updateRate;
    }

    if (sShrinkWindowCurrentVal < sShrinkWindowVal) {
        if (D_8012CED0 != 1) {
            D_8012CED0 = 1;
        }

        if (sShrinkWindowCurrentVal + off < sShrinkWindowVal) {
            sShrinkWindowCurrentVal += off;
        } else {
            sShrinkWindowCurrentVal = sShrinkWindowVal;
        }
    } else if (sShrinkWindowVal < sShrinkWindowCurrentVal) {
        if (D_8012CED0 != 2) {
            D_8012CED0 = 2;
        }

        if (sShrinkWindowVal < sShrinkWindowCurrentVal - off) {
            sShrinkWindowCurrentVal -= off;
        } else {
            sShrinkWindowCurrentVal = sShrinkWindowVal;
        }
    } else {
        D_8012CED0 = 0;
    }

    if (HREG(80) == 0x13) {
        if (HREG(94) != 0x13) {
            HREG(94) = 0x13;
            HREG(81) = 0;
            HREG(82) = 0;
            HREG(83) = 0;
            HREG(84) = 0;
            HREG(85) = 0;
            HREG(86) = 0;
            HREG(87) = 0;
            HREG(88) = 0;
            HREG(89) = 0;
        }
        HREG(83) = D_8012CED0;
        HREG(84) = sShrinkWindowCurrentVal;
        HREG(85) = sShrinkWindowVal;
        HREG(86) = off;
    }
}
