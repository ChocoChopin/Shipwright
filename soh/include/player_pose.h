#ifndef PLAYER_POSE_H
#define PLAYER_POSE_H
#include "z64player.h"
#include <stddef.h>

/* One synchronous canonical actor draw. The packet is allocated in the current
 * graphics arena: matrices stay alive through this transaction's submission,
 * never across frames or scene/player replacement. No live gameplay is owned. */
typedef struct {
    Mtx matrices[PLAYER_LIMB_MAX];
    struct {
        Gfx* mesh;
        u8 matrixIndex;
        u8 root;
    } limbs[PLAYER_LIMB_MAX];
    Gfx* bracelet;
    const Player* playerIdentity;
    const struct PlayState* sceneIdentity;
    u32 frame;
    s16 scene;
    u8 limbCount;
    u8 matrixCount;
} PlayerPosePacket;

u32 Player_PoseUnexpectedState2(const Player* player);
void Player_FormatState2(u32 value, u32 unexpected, char* output, size_t capacity);
const char* Player_PoseProfileRejection(struct PlayState* play, const Player* player);
int Player_IsPoseActionAdmitted(const Player* player);
int Player_IsPoseProfileAdmitted(struct PlayState* play, const Player* player);
void Player_AdvancePoseContactsLegacy(struct PlayState* play, Player* player, PlayerPosePacket* packet, s32 lod);
void Player_AdvanceIntermediatePose(struct PlayState* play, Player* player, PlayerPosePacket* packet, s32 lod);
void* Player_DrawPosePresentation(const void* packet, void* output, void* paint);
size_t Player_CopyPoseStatics(void* output);
const char* GameInteractor_PlayerPoseHookRejection(void);
unsigned int CustomEquipment_GetPlayerPoseHook(void);
const char* CustomEquipment_PlayerPoseResourceRejection(void);
#endif
