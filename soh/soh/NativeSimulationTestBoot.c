#include "global.h"
#include "soh/NativeSimulationTest.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

void Sram_InitDebugSave(void);

/* Test fixture construction only. These are the existing TitleSetup, SRAM and
 * scene-select/debug-save entry semantics, without running a title demo first.
 * The ordinary boot path never calls this function. No runtime save is loaded.
 */
void NativeSimTest_BootSave(void) {
    s32 i;

    if (!NativeSimTest_IsEnabled()) {
        return;
    }

    SaveContext_Init();
    Sram_InitSram(NULL);
    gSaveContext.fileNum = 0xFF;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    /* InitDebugSave chooses age-appropriate equipment during initialization. */
    gSaveContext.linkAge = NativeSimTest_ConfigInt("age", 1);
    gSaveContext.language = LANGUAGE_ENG;
    gSaveContext.audioSetting = 0;
    gSaveContext.zTargetSetting = 0;
    Sram_InitDebugSave();
    gSaveContext.magicFillTarget = gSaveContext.magic;
    gSaveContext.magic = 0;
    gSaveContext.magicCapacity = 0;
    gSaveContext.magicLevel = 0;
    gSaveContext.sceneLayer = 0;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.nightFlag = 0;
    gSaveContext.skyboxTime = gSaveContext.dayTime = 0x8000;
    gSaveContext.entranceIndex = NativeSimTest_ConfigInt("entrance", ENTR_LINKS_HOUSE_CHILD_SPAWN);
    gSaveContext.respawnFlag = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = ENTR_LOAD_OPENING;
    for (i = 0; i < ARRAY_COUNT(gSaveContext.buttonStatus); ++i) {
        gSaveContext.buttonStatus[i] = BTN_ENABLED;
    }
    gSaveContext.forceRisingButtonAlphas = 0;
    gSaveContext.nextHudVisibilityMode = 0;
    gSaveContext.hudVisibilityMode = 0;
    gSaveContext.hudVisibilityModeTimer = 0;
    Audio_SetSoundOutputMode(gSaveContext.audioSetting);
    Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_STOP);
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gWeatherMode = 0;
    GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum);
    NativeSimTest_Event("fixture_boot", "NativeSimTest_BootSave", gSaveContext.entranceIndex);
}
