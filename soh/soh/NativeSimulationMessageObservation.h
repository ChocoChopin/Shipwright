#ifndef NATIVE_SIMULATION_MESSAGE_OBSERVATION_H
#define NATIVE_SIMULATION_MESSAGE_OBSERVATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

// Last CPU Message_Draw observation, not GPU completion. glyphFingerprint is
// ordered FNV-1a64 of emitted ASCII glyph metadata, each integer encoded as four
// little-endian bytes: codepoint, X/Y, RGBA, texture size/scale, shadow/offset.
// Spaces/control bytes do not emit glyphs. iconType is the last emitted icon:
// -1 none, -2 unknown resource, otherwise TextBoxIcon. Unsupported paths mark
// asciiPathComplete=0; no Japanese/ocarina/credits coverage is implied.
// iconFingerprint uses the same word encoding for each emitted X/Y/type,
// primitive RGBA, environment RGBA, texture size/scale after legacy flash work.
typedef struct NativeSimMessagePaintObservation {
    int32_t observed, entryMode, entryTextDrawPos, entryEndType, entryIconBranch;
    int32_t asciiPathComplete, displayListLinked, glyphCount, iconCount, iconType, iconX, iconY;
    uint32_t drawFrame;
    uint64_t glyphFingerprint, iconFingerprint;
} NativeSimMessagePaintObservation;

// Read-only diagnostic values. No pointers, text payloads, restore operation, or
// relocated message state; schema users must serialize members explicitly.
typedef struct NativeSimMessageObservation {
    int32_t textId, choiceTextId, msgLength, msgMode, derivedState;
    int32_t textBoxProperties, textBoxType, textBoxPos;
    int32_t msgBufPos, textDrawPos, decodedTextLen, textUnskippable;
    int32_t textDelay, textDelayTimer, stateTimer, textboxEndType, choiceIndex, choiceNum;
    int32_t textPosX, textPosY, textColorR, textColorG, textColorB, textColorAlpha;
    int32_t textboxColorRed, textboxColorGreen, textboxColorBlue;
    int32_t textboxColorAlphaCurrent, textboxColorAlphaTarget;
    int32_t ocarinaMode, ocarinaAction, hasTalkActor;
    int32_t startFrameCount, textboxSkipped, nextTextId, textBoxNum;
    int32_t textFade, textIsCredits, messageHasSetSfx, lastPlayedSong;
    int32_t lastLanguage, displayAsEnglish, language;
    int32_t textboxX, textboxY, textboxWidth, textboxHeight, textboxTexWidth, textboxTexHeight;
    int32_t textboxXTarget, textboxYTarget, textboxWidthTarget, textboxHeightTarget;
    int32_t textboxTexWidthTarget, textboxTexHeightTarget, textboxEndX, textboxEndY;
    int32_t textInitX, textInitY, textLineSpacing, textCharScale, yreg15, yreg31;
    int32_t actionState, actionCurrent, actionTarget, actionOverride, actionOverrideId;
    float actionRotation;
    int32_t hudVisibilityMode, prevHudVisibilityMode;
    // decodedTextLen is -1 unless decodedBufferValid: opening/next/closing and
    // non-text modes do not expose stale or not-yet-initialized decoded storage.
    int32_t rawBufferValid, decodedBufferValid;
    uint64_t rawBufferFingerprint, decodedBufferFingerprint;
    NativeSimMessagePaintObservation paint;
} NativeSimMessageObservation;

void Message_GetNativeSimObservation(struct PlayState* play, NativeSimMessageObservation* out);

#ifdef __cplusplus
}
#endif
#endif
