#pragma once
#include "z64.h"
/* Intermediate Player service: acquisition has no retrace/rumble/world timers;
 * a rejected profile preserves accumulated menu/world input for the next world.
 * Caller owns simulation-availability time and whole-profile admission. */
s32 PadMgr_PollPlayer(PadMgr* padmgr, u64 replayTimeQ);
void PadMgr_ProcessPlayerInputs(PadMgr* padmgr);
s32 PadMgr_GetPlayerSample(PadMgr* padmgr, Input* input, s32 consume);
s32 PadMgr_ConsumePlayerSample(Input* accumulated, Input* input, s32 consume);
