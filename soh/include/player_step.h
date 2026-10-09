#pragma once
#include "z64.h"
#include "z64player.h"

/* Complete pre-mutation admission for one bounded Player interval. The caller
 * must also establish synchronous resource/presentation storage and host service.
 * A rejection preserves input and all live Player state. */
const char* Player_HighRateProfileRejection(struct PlayState* play, Player* player,
                                           const Input* input, unsigned quanta, int worldBoundary);
const char* Player_HighRateContactRejection(struct PlayState* play, const Player* player);
const char* GameInteractor_PlayerRateHookRejection(void);
void Player_AdvanceIntermediate(struct PlayState* play, Player* player, const Input* input);
