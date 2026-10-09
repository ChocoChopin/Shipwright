#include "GameInteractor_Hooks.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

// MARK: - Gameplay

void GameInteractor_ExecuteOnZTitleInit(void* gameState) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnZTitleInit>(gameState);
}

void GameInteractor_ExecuteOnZTitleUpdate(void* gameState) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnZTitleUpdate>(gameState);
}

void GameInteractor_ExecuteOnLoadGame(int32_t fileNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLoadGame>(fileNum);
}

void GameInteractor_ExecuteOnExitGame(int32_t fileNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnExitGame>(fileNum);
}

void GameInteractor_ExecuteOnGameStateMainStart() {
    // Cleanup all hooks at the start of each frame
    GameInteractor::Instance->RemoveAllQueuedHooks();

    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnGameStateMainStart>();
}

void GameInteractor_ExecuteOnGameFrameUpdate() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnGameFrameUpdate>();
}

void GameInteractor_ExecuteOnCameraState(PlayState* play) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCameraState>(play);
}

void GameInteractor_ExecuteOnItemReceiveHooks(GetItemEntry itemEntry) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnItemReceive>(itemEntry);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnItemReceive>(itemEntry);
}

void GameInteractor_ExecuteOnEquipmentDelete(int16_t equipmentType, uint16_t equipValue) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnEquipmentDelete>(equipmentType, equipValue);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnEquipmentDelete>(equipmentType, equipValue);
}

void GameInteractor_ExecuteOnSaleEndHooks(GetItemEntry itemEntry) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSaleEnd>(itemEntry);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnSaleEnd>(itemEntry);
}

void GameInteractor_ExecuteOnTransitionEndHooks(int16_t sceneNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnTransitionEnd>(sceneNum);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnTransitionEnd>(sceneNum, sceneNum);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnTransitionEnd>(sceneNum);
}

void GameInteractor_ExecuteOnSceneInit(int16_t sceneNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneInit>(sceneNum);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnSceneInit>(sceneNum, sceneNum);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnSceneInit>(sceneNum);
}

void GameInteractor_ExecuteAfterSceneCommands(int16_t sceneNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::AfterSceneCommands>(sceneNum);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::AfterSceneCommands>(sceneNum, sceneNum);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::AfterSceneCommands>(sceneNum);
}

void GameInteractor_ExecuteOnSceneFlagSet(int16_t sceneNum, int16_t flagType, int16_t flag) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneFlagSet>(sceneNum, flagType, flag);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnSceneFlagSet>(sceneNum, flagType, flag);
}

void GameInteractor_ExecuteOnSceneFlagUnset(int16_t sceneNum, int16_t flagType, int16_t flag) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneFlagUnset>(sceneNum, flagType, flag);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnSceneFlagUnset>(sceneNum, flagType, flag);
}

void GameInteractor_ExecuteOnFlagSet(int16_t flagType, int16_t flag) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnFlagSet>(flagType, flag);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnFlagSet>(flagType, flag);
}

void GameInteractor_ExecuteOnFlagUnset(int16_t flagType, int16_t flag) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnFlagUnset>(flagType, flag);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnFlagUnset>(flagType, flag);
}

void GameInteractor_ExecuteOnSceneSpawnActors() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneSpawnActors>();
}

void GameInteractor_ExecuteOnLinkSkeletonInit() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLinkSkeletonInit>();
}

void GameInteractor_ExecuteOnLinkEquipmentChange() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLinkEquipmentChange>();
}

void GameInteractor_ExecuteOnPlayerUpdate() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerUpdate>();
}

void GameInteractor_ExecuteOnSetDoAction(uint16_t action) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSetDoAction>(action);
}

void GameInteractor_ExecuteOnPlayerSfx(u16 sfxId) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerSfx>(sfxId);
}

void GameInteractor_ExecuteOnOcarinaSongAction() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnOcarinaSongAction>();
}

void GameInteractor_ExecuteOnWarpSongLeave() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnWarpSongLeave>();
}

void GameInteractor_ExecuteOnOcarinaNote(uint8_t note, float modulator, int8_t bend) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnOcarinaNote>(note, modulator, bend);
}

void GameInteractor_ExecuteOnShopSlotChangeHooks(uint8_t cursorIndex, int16_t price) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnShopSlotChange>(cursorIndex, price);
}

void GameInteractor_ExecuteOnDungeonKeyUsedHooks(uint16_t mapIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnDungeonKeyUsed>(mapIndex);
}

bool GameInteractor_ShouldActorInit(void* actor) {
    bool result = true;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::ShouldActorInit>(actor, &result);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::ShouldActorInit>(((Actor*)actor)->id, actor, &result);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::ShouldActorInit>((uintptr_t)actor, actor, &result);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::ShouldActorInit>(actor, &result);
    return result;
}

void GameInteractor_ExecuteOnActorInit(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorInit>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorInit>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorInit>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorInit>(actor);
}

void GameInteractor_ExecuteOnActorSpawn(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorSpawn>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorSpawn>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorSpawn>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorSpawn>(actor);
}

bool GameInteractor_ShouldActorUpdate(void* actor) {
    bool result = true;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::ShouldActorUpdate>(actor, &result);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::ShouldActorUpdate>(((Actor*)actor)->id, actor, &result);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::ShouldActorUpdate>((uintptr_t)actor, actor, &result);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::ShouldActorUpdate>(actor, &result);
    return result;
}

void GameInteractor_ExecuteOnActorUpdate(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorUpdate>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorUpdate>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorUpdate>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorUpdate>(actor);
}

void GameInteractor_ExecuteOnActorKill(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorKill>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorKill>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorKill>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorKill>(actor);
}

bool GameInteractor_ShouldActorDestroy(void* actor) {
    bool result = true;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::ShouldActorDestroy>(actor, &result);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::ShouldActorDestroy>(((Actor*)actor)->id, actor,
                                                                                    &result);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::ShouldActorDestroy>((uintptr_t)actor, actor, &result);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::ShouldActorDestroy>(actor, &result);
    return result;
}

void GameInteractor_ExecuteOnActorDestroy(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorDestroy>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorDestroy>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorDestroy>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorDestroy>(actor);
}

void GameInteractor_ExecuteOnEnemyDefeat(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnEnemyDefeat>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnEnemyDefeat>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnEnemyDefeat>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnEnemyDefeat>(actor);
}

void GameInteractor_ExecuteOnBossDefeat(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnBossDefeat>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnBossDefeat>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnBossDefeat>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnBossDefeat>(actor);
}

void GameInteractor_ExecuteOnTimestamp(u8 item) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnTimestamp>(item);
}

void GameInteractor_ExecuteOnPlayerBonk() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerBonk>();
}

void GameInteractor_ExecuteOnPlayerSetModels(Player* player, u8 modelGroup) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerSetModels>(player, modelGroup);
}

void GameInteractor_ExecuteOnPlayerHealthChange(int16_t amount) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerHealthChange>(amount);
}

void GameInteractor_ExecuteOnPlayerBottleUpdate(int16_t contents) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerBottleUpdate>(contents);
}

void GameInteractor_ExecuteOnPlayerHoldUpShield() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerHoldUpShield>();
}

void GameInteractor_ExecuteOnPlayerFirstPersonControl(Player* player) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerFirstPersonControl>(player);
}

void GameInteractor_ExecuteOnPlayerShieldControl(float* sp50, float* sp54) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerShieldControl>(sp50, sp54);
}

void GameInteractor_ExecuteOnPlayerProcessStick() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerProcessStick>();
}

void GameInteractor_ExecuteOnPlayDestroy() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayDestroy>();
}

void GameInteractor_ExecuteOnPlayDrawBegin() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayDrawBegin>();
}

void GameInteractor_ExecuteOnPlayDrawEnd() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayDrawEnd>();
}

// Read-only preflight. Unknown global/filter hooks may affect either behavior.
extern "C" unsigned int CustomEquipment_GetPlayerPoseHook(void);
extern "C" const char* CustomEquipment_PlayerPoseResourceRejection(void);
void TimeSaverOnVanillaBehaviorHandler(GIVanillaBehavior id, bool* should, va_list args);
extern "C" const char* GameInteractor_PlayerPoseHookRejection(void) {
    using Hooks = GameInteractor::RegisteredGameHooks<GameInteractor::OnVanillaBehavior>;
    // This built-in dispatcher's default case is a no-op for both pose flags.
    // Match the actual function target; never exempt arbitrary global handlers.
    using Handler = void (*)(GIVanillaBehavior, bool*, va_list);
    for (const auto& entry : Hooks::functions) {
        const auto* target = entry.second.target<Handler>();
        if (!target || *target != TimeSaverOnVanillaBehaviorHandler) return "global vanilla behavior hook";
    }
    if (!Hooks::functionsForFilter.empty()) return "filtered vanilla behavior hook";
    if (const char* resource = CustomEquipment_PlayerPoseResourceRejection()) return resource;
    for (auto flag : {VB_PLAYER_OVERRIDE_LIMB_DRAW, VB_DRAW_ADDITIONAL_RETICLES}) {
        auto it = Hooks::functionsForID.find(flag);
        if (it == Hooks::functionsForID.end()) continue;
        for (const auto& entry : it->second) {
            if (flag != VB_PLAYER_OVERRIDE_LIMB_DRAW || entry.first != CustomEquipment_GetPlayerPoseHook())
                return flag == VB_PLAYER_OVERRIDE_LIMB_DRAW ? "custom limb hook" : "reticle hook";
        }
    }
    return nullptr;
}

extern "C" void Mouse_QuickspinBehaviorHandler(GIVanillaBehavior, bool*, va_list);
void ExtraTraps_GiveItemHandler(GIVanillaBehavior, bool*, va_list);
bool ExtraTraps_PlayerRateInactive();
unsigned int RocsFeather_PlayerUseItemHook();
extern "C" const char* GameInteractor_PlayerRateHookRejection(void) {
    if (const char* reason = GameInteractor_PlayerPoseHookRejection()) return reason;
    using Hooks = GameInteractor::RegisteredGameHooks<GameInteractor::OnVanillaBehavior>;
    // Conservative superset of admitted action/collision decision hooks. Omit
    // hooks confined to initialization, hookshot, first person, ladders, carried
    // actors and drinking: those paths cannot enter this pre-admitted closure.
    // Unknown handlers are never invoked speculatively to discover mutation.
    const GIVanillaBehavior decisions[] = {
        VB_AFTER_PROCESS_SCENE_COLLISION, VB_AFTER_ACTOR_UPDATE_BGCHECKINFO,
        VB_ALLOW_QUICK_PUTAWAY, VB_BE_ABLE_TO_OPEN_DOORS, VB_BOTTLE_ACTOR, VB_BURN_SHIELD,
        VB_CHANGE_HELD_ITEM_AND_USE_ITEM, VB_CLIMB, VB_CRAWL,
        VB_CRAWL_SPEED_ENTER, VB_CRAWL_SPEED_EXIT, VB_CRAWL_SPEED_INCREASE,
        VB_DEKU_STICK_BE_ON_FIRE, VB_DEKU_STICK_BREAK, VB_DEKU_STICK_BURN_DOWN, VB_DEKU_STICK_BURN_OUT,
        VB_DOOR_PLAY_SCENE_TRANSITION, VB_EMPTYING_BOTTLE,
        VB_EXECUTE_PLAYER_ACTION_FUNC, VB_FISHING_ZERO_XZ,
        VB_GIVE_ITEM_FROM_CHEST,
        VB_ITEM_ACTION_BE_NONE, VB_MOVE_THROWN_ACTOR, VB_NOT_CAST_FISHING,
        VB_OPEN_CHEST, VB_OVERRIDE_BUTTON_ITEM_USED, VB_PLAYER_AIM_WITH_LEFT_STICK,
        VB_PLAYER_ARROW_MAGIC_CONSUMPTION, VB_PLAYER_FIRST_PERSON_ALIGN_YAW, VB_PLAYER_FIRST_PERSON_DECELERATE,
        VB_PLAYER_LIMIT_DIVE_XZ_SPEED, VB_PLAYER_LIMIT_JUMP_SPEED, VB_PLAYER_MODIFY_RUN_SPEED,
        VB_PLAYER_MODIFY_SWIM_SPEED, VB_PLAYER_ROLL_CHAIN, VB_PLAYER_ROLL_STEER, VB_PLAYER_SPAWN_SWIMMING,
        VB_PLAYER_UNEQUIP_MASK_WITHOUT_BUTTON, VB_PLAY_BEAN_PLANTING_CS, VB_PLAY_NABOORU_CAPTURED_CS,
        VB_PLAY_SLOW_CHEST_CS, VB_PLAY_THROW_ANIMATION,
        VB_PREVENT_STRENGTH, VB_PUTAWAY_BECAUSE_DISABLED_ITEM_BUTTONS, VB_RECIEVE_FALL_DAMAGE,
        VB_REVALIDATE_CLIMBED_WALL, VB_RUMBLE_FOR_SECRET, VB_SET_IDLE_ANIM, VB_SET_STATIC_FLOOR_TYPE,
        VB_SET_STATIC_PREV_FLOOR_TYPE, VB_SET_VOIDOUT_FROM_SURFACE, VB_SHORT_CIRCUIT_GIVE_ITEM_PROCESS,
        VB_SHOULD_QUICKSPIN, VB_SHOW_MASTER_SWORD_TO_PLACE_IN_PEDESTAL,
        VB_SKIP_TALKING, VB_SPEAK, VB_SURFACE_ANGLE_IS_CLIMBABLE, VB_THROW_OR_PUT_DOWN_HELD_ITEM,
        VB_TOGGLE_Z_TARGET_SWITCH_DIRECTION, VB_TOGGLE_Z_TARGET_SWITCH_TARGETS, VB_TRIGGER_VOIDOUT,
        VB_USE_HELD_ITEM_AFTER_CHANGE
    };
    for (auto flag : decisions) {
        auto it = Hooks::functionsForID.find(flag);
        if (it == Hooks::functionsForID.end()) continue;
        for (const auto& entry : it->second) {
            using Handler = void (*)(GIVanillaBehavior, bool*, va_list);
            const auto* handler = entry.second.target<Handler>();
            // The disabled built-in only writes the caller's local false result;
            // it cannot touch mouse gesture state or force a spin. Match code
            // identity, not registration count or merely the hook category.
            if (flag == VB_SHOULD_QUICKSPIN && handler && *handler == Mouse_QuickspinBehaviorHandler &&
                !CVarGetInteger(CVAR_SETTING("EnableMouse"),0)) continue;
            if (flag == VB_SHORT_CIRCUIT_GIVE_ITEM_PROCESS && handler && *handler == ExtraTraps_GiveItemHandler &&
                ExtraTraps_PlayerRateInactive()) continue;
            // The complete input/equipment gate admits only the Kokiri sword
            // button. This exact built-in is inert for every other item ID.
            if (flag == VB_CHANGE_HELD_ITEM_AND_USE_ITEM && entry.first == RocsFeather_PlayerUseItemHook()) continue;
            static thread_local std::string rejection;
            rejection = "custom Player behavior hook " + std::to_string(static_cast<int>(flag));
            return rejection.c_str();
        }
    }
    if (!GameInteractor::RegisteredGameHooks<GameInteractor::OnPlayerSetModels>::functions.empty())
        return "custom Player model-selection hook";
    // The admitted global TimeSaver dispatcher has no reachable active cases:
    // its four Player cases require initialization, beans, chests or a cutscene.
    return nullptr;
}

bool GameInteractor_Should(GIVanillaBehavior flag, u32 result, ...) {
    // Only the external function can use the Variadic Function syntax
    // To pass the va args to the next caller must be done using va_list and reading the args into it
    // Because there can be N subscribers registered to each template call, the subscribers will be responsible for
    // creating a copy of this va_list to avoid incrementing the original pointer between calls
    va_list args;
    va_start(args, result);

    // Because of default argument promotion, even though our incoming "result" is just a bool, it needs to be typed as
    // an int to be permitted to be used in `va_start`, otherwise it is undefined behavior.
    // Here we downcast back to a bool for our actual hook handlers
    bool boolResult = static_cast<bool>(result);

    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnVanillaBehavior>(flag, &boolResult, args);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnVanillaBehavior>(flag, flag, &boolResult, args);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnVanillaBehavior>(flag, &boolResult, args);

    va_end(args);
    return boolResult;
}

// MARK: -  Save Files

void GameInteractor_ExecuteOnSaveFile(int32_t fileNum, int32_t sectionID) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSaveFile>(fileNum, sectionID);
}

void GameInteractor_ExecuteOnLoadFile(int32_t fileNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLoadFile>(fileNum);
}

void GameInteractor_ExecuteOnDeleteFile(int32_t fileNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnDeleteFile>(fileNum);
}

// MARK: - Dialog

void GameInteractor_ExecuteOnDialogMessage() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnDialogMessage>();
}

void GameInteractor_ExecuteOnPresentTitleCard() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPresentTitleCard>();
}

void GameInteractor_ExecuteOnInterfaceUpdate() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnInterfaceUpdate>();
}

void GameInteractor_ExecuteOnKaleidoscopeUpdate(int16_t inDungeonScene) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoscopeUpdate>(inDungeonScene);
}

void GameInteractor_ExecuteOnMinimapDrawCompassIcons() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnMinimapDrawCompassIcons>();
}

// MARK: - Main Menu

void GameInteractor_ExecuteOnPresentFileSelect() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPresentFileSelect>();
}

void GameInteractor_ExecuteOnUpdateFileSelectSelection(uint16_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileSelectSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileSelectConfirmationSelection(uint16_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileSelectConfirmationSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileCopySelection(uint16_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileCopySelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileCopyConfirmationSelection(uint16_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileCopyConfirmationSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileEraseSelection(uint16_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileEraseSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileEraseConfirmationSelection(uint16_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileEraseConfirmationSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileAudioSelection(uint8_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileAudioSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileTargetSelection(uint8_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileTargetSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileLanguageSelection(uint8_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileLanguageSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileQuestSelection(uint8_t questIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileQuestSelection>(questIndex);
}

void GameInteractor_ExecuteOnUpdateFileBossRushOptionSelection(uint8_t optionIndex, uint8_t optionValue) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileBossRushOptionSelection>(optionIndex,
                                                                                                optionValue);
}

void GameInteractor_ExecuteOnUpdateFileRandomizerOptionSelection(uint8_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileRandomizerOptionSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileNameSelection(int16_t charCode) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileNameSelection>(charCode);
}

void GameInteractor_ExecuteOnFileChooseMain(void* gameState) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnFileChooseMain>(gameState);
}

// MARK: - Game

void GameInteractor_ExecuteOnSetGameLanguage() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSetGameLanguage>();
}

// MARK: - System

void GameInteractor_RegisterOnAssetAltChange(void (*fn)(void)) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnAssetAltChange>(fn);
}

// MARK: Pause Menu

void GameInteractor_ExecuteOnKaleidoUpdate() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoUpdate>();
}

// MARK: Messages
void GameInteractor_ExecuteOnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnOpenText>(textId, loadFromMessageTable);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnOpenText>(*textId, textId, loadFromMessageTable);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnOpenText>(textId, loadFromMessageTable);
}

// Mark: Audio
void GameInteractor_ExecuteOnSeqPlayerInit(int32_t playerIdx, int32_t seqId) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSeqPlayerInit>(playerIdx, seqId);
}

// MARK: - Rando
void GameInteractor_ExecuteOnRandoEntranceDiscovered(u16 entranceIndex, u8 isReversedEntrance) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnRandoEntranceDiscovered>(entranceIndex,
                                                                                      isReversedEntrance);
}
