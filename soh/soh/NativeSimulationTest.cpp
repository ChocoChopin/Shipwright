// Opt-in observational 20-Hz replay. No gameplay arithmetic lives in this module.
#include "NativeSimulationTest.hpp"
#include "NativeSimulationPresentation.h"
#include "NativeSimulationPresentationCoverage.hpp"
#include "NativeSimulationHudObservation.h"
#include "NativeSimulationMessageObservation.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/gameconsole.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
extern "C" {
#include "global.h"
#include "player_pose.h"
extern EffectContext sEffectContext;
extern EffectSsInfo sEffectSsInfo;
#include "regs.h"
#include "message_data_textbox_types.h"
#include "overlays/actors/ovl_En_Kanban/z_en_kanban.h"
}

using nlohmann::json;
namespace {
bool enabled = false, measuring = false, verbose = false;
bool verifyPresentationPurity = false, purityNegativeControl = false;
json purityCoverage = json::object(), admissionCoverage = json::object();
json fixture, previousPhase;
std::filesystem::path output;
std::ofstream snapshots, trace;
std::vector<std::string> pendingTrace;
uint64_t tick = 0, setupFrames = 0, engineFrames = 0, audioBlocks = 0, audioSamples = 0, sequence = 0;
uint64_t spawnOrdinal = 0, sceneEpoch = 0;
uint32_t audioClockCalls = 0;
int updateCalls = 0, drawCalls = 0;
uint64_t playerPoseGeneration = 0, playerContacts = 0;
std::string phase = "initialization";
std::unordered_map<Actor*, uint64_t> actorIds;
Actor* currentActor = nullptr;
std::vector<Actor*> actorScope;
struct Stream { uint64_t calls = 0, drawCalls = 0; uint32_t state = 0; uint64_t order = 14695981039346656037ull; };
std::map<std::string, Stream> streams;

std::string Hex(uint64_t value, int width) {
    std::ostringstream str;
    str << std::hex << std::setfill('0') << std::setw(width) << value;
    return str.str();
}
[[noreturn]] void Fail(const std::string& message) {
    if (!output.empty() && !std::filesystem::exists(output / "result.json")) {
        std::ofstream file(output / "result.json");
        file << json{{"schema", 1}, {"status", "fail"}, {"error", message}, {"tick", tick},
                     {"phase", phase}, {"ticks_completed", tick}}.dump(2) << '\n';
    }
    if (snapshots.is_open()) snapshots.flush();
    if (trace.is_open()) trace.flush();
    fprintf(stderr, "Native simulation test failed at tick %llu (%s): %s\n",
            static_cast<unsigned long long>(tick), phase.c_str(), message.c_str());
    std::exit(2);
}
json Float(float value) {
    if (!std::isfinite(value)) Fail("non-finite semantic float");
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return {{"bits", Hex(bits, 8)}, {"value", value}};
}
json Vec(const Vec3f& value) { return {{"x", Float(value.x)}, {"y", Float(value.y)}, {"z", Float(value.z)}}; }
json Rot(const Vec3s& value) { return {{"x", value.x}, {"y", value.y}, {"z", value.z}}; }
json ActorId(Actor* actor) {
    if (!actor) return nullptr;
    auto it = actorIds.find(actor);
    if (it == actorIds.end()) return "untracked";
    return "scene" + std::to_string(sceneEpoch) + ":spawn" + std::to_string(it->second);
}
json Polygon(CollisionPoly* poly) {
    if (!poly) return nullptr;
    // Semantic geometry, not an address or allocator-dependent polygon ordinal.
    return {{"type", poly->type}, {"vertices", {poly->flags_vIA, poly->flags_vIB, poly->vIC}},
            {"normal", Rot(poly->normal)}, {"dist", poly->dist}};
}
json ActorState(Actor* actor) {
    return {{"identity", ActorId(actor)}, {"type", actor->id}, {"params", actor->params},
            {"category", actor->category}, {"room", actor->room}, {"flags", actor->flags},
            {"position", Vec(actor->world.pos)}, {"previous_position", Vec(actor->prevPos)},
            {"rotation", Rot(actor->world.rot)}, {"shape_rotation", Rot(actor->shape.rot)},
            {"focus", Vec(actor->focus.pos)}, {"velocity", Vec(actor->velocity)},
            {"speed_xz", Float(actor->speedXZ)}, {"gravity", Float(actor->gravity)},
            {"min_velocity_y", Float(actor->minVelocityY)}, {"scale", Vec(actor->scale)},
            {"bg_flags", actor->bgCheckFlags}, {"floor_height", Float(actor->floorHeight)},
            {"floor_bg_id", actor->floorBgId}, {"wall_bg_id", actor->wallBgId},
            {"floor", Polygon(actor->floorPoly)}, {"wall", Polygon(actor->wallPoly)},
            {"wall_yaw", actor->wallYaw}, {"health", actor->colChkInfo.health},
            {"damage", actor->colChkInfo.damage}, {"displacement", Vec(actor->colChkInfo.displacement)},
            {"freeze_timer", actor->freezeTimer}, {"color_filter_timer", actor->colorFilterTimer},
            {"is_drawn", actor->isDrawn}, {"initializing", actor->init != nullptr},
            {"alive", actor->update != nullptr}, {"has_draw", actor->draw != nullptr},
            {"parent", ActorId(actor->parent)}, {"child", ActorId(actor->child)}, {"sfx", actor->sfx}};
}
json Animation(SkelAnime& animation) {
    std::string resource = "unmapped";
    if (animation.animation && ResourceMgr_OTRSigCheck(static_cast<char*>(animation.animation)))
        resource = static_cast<const char*>(animation.animation);
    return {{"resource", resource}, {"mode", animation.mode}, {"frame", Float(animation.curFrame)},
            {"start", Float(animation.startFrame)}, {"end", Float(animation.endFrame)},
            {"length", Float(animation.animLength)}, {"speed", Float(animation.playSpeed)},
            {"morph_weight", Float(animation.morphWeight)}, {"morph_rate", Float(animation.morphRate)},
            {"movement_flags", animation.movementFlags}, {"previous_translation", Rot(animation.prevTransl)},
            {"previous_rotation", animation.prevRot}};
}
json PlayerState(Player* player) {
    json state = ActorState(&player->actor);
    state["action"] = NativeSimTest_PlayerActionName(player);
    state["animation"] = Animation(player->skelAnime);
    state["upper_animation"] = Animation(player->upperSkelAnime);
    state["linear_velocity"] = Float(player->linearVelocity);
    state["yaw"] = player->yaw;
    state["state_flags"] = {player->stateFlags1, player->stateFlags2, player->stateFlags3};
    state["action_variables"] = {player->av1.actionVar1, player->av2.actionVar2};
    state["invincibility_timer"] = player->invincibilityTimer;
    state["target_timer"] = player->zTargetActiveTimer;
    state["target"] = ActorId(player->focusActor);
    state["held_actor"] = ActorId(player->heldActor);
    state["door_timer"] = player->doorTimer;
    state["fall_distance"] = player->fallDistance;
    state["fall_start_height"] = player->fallStartHeight;
    state["melee_state"] = player->meleeWeaponState;
    state["melee_animation"] = player->meleeWeaponAnimation;
    state["cs_action"] = player->csAction;
    state["damage_flicker"] = player->damageFlickerAnimCounter;
    state["left_hand"] = Vec(player->leftHandPos);
    state["body_parts"] = json::array();
    for (auto& pos : player->bodyPartsPos) state["body_parts"].push_back(Vec(pos));
    state["weapon_geometry"] = json::array();
    for (auto& weapon : player->meleeWeaponInfo)
        state["weapon_geometry"].push_back({{"active", weapon.active}, {"tip", Vec(weapon.tip)}, {"base", Vec(weapon.base)}});
    state["shield_quad"] = json::array();
    for (auto& pos : player->shieldQuad.dim.quad) state["shield_quad"].push_back(Vec(pos));
    return state;
}
json ColliderState(const Collider& c, PlayState* play) {
    json state = {{"actor", ActorId(c.actor)}, {"at_actor", ActorId(c.at)}, {"ac_actor", ActorId(c.ac)},
        {"oc_actor", ActorId(c.oc)}, {"at_flags", c.atFlags}, {"ac_flags", c.acFlags},
        {"oc_flags", {c.ocFlags1, c.ocFlags2}}, {"shape", c.shape}, {"material", c.colType}};
    auto indices = [&](Collider** list, int count) {
        json result = json::array();
        for (int i = 0; i < count; ++i) if (list[i] == &c) result.push_back(i);
        return result;
    };
    state["registered_at"] = indices(play->colChkCtx.colAT, play->colChkCtx.colATCount);
    state["registered_ac"] = indices(play->colChkCtx.colAC, play->colChkCtx.colACCount);
    state["registered_oc"] = indices(play->colChkCtx.colOC, play->colChkCtx.colOCCount);
    return state;
}
json ColliderElement(const ColliderInfo& info) {
    return {{"touch_flags", info.toucherFlags}, {"bump_flags", info.bumperFlags}, {"oc_flags", info.ocElemFlags},
        {"damage_flags", info.toucher.dmgFlags}, {"damage", info.toucher.damage}, {"effect", info.toucher.effect},
        {"accept_flags", info.bumper.dmgFlags}, {"defense", info.bumper.defense},
        {"hit_position", Rot(info.bumper.hitPos)}};
}
json QuadState(const ColliderQuad& quad, PlayState* play) {
    auto state = ColliderState(quad.base, play);
    state["element"] = ColliderElement(quad.info);
    state["vertices"] = json::array();
    for (const auto& v : quad.dim.quad) state["vertices"].push_back(Vec(v));
    state["nearest_distance"] = Float(quad.dim.acDist);
    return state;
}
json MatrixState(const MtxF& matrix) {
    json result = json::array();
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) result.push_back(Float(matrix.mf[i][j]));
    return result;
}
json PlayerDetail(PlayState* play) {
    auto* p = GET_PLAYER(play);
    if (!p) return nullptr;
    json state = {{"pose_generation", playerPoseGeneration}, {"contact_count", playerContacts},
        {"combo_count", p->unk_845}, {"combo_timer", p->unk_844},
        {"held_item_action", p->heldItemAction}, {"item_action", p->itemAction},
        {"model_group", p->modelGroup}, {"hand_types", {p->leftHandType, p->rightHandType}},
        {"shield", p->currentShield}, {"parallel_yaw", p->parallelYaw}, {"head_rotation", Rot(p->headLimbRot)},
        {"upper_rotation", Rot(p->upperLimbRot)}, {"upper_yaw", p->upperLimbYawSecondary},
        {"root_tilt", p->unk_6C2}, {"root_offset", Float(p->unk_6C4)},
        {"attachment_rotation", Rot(p->unk_3BC)}, {"hookshot_position", Vec(p->unk_3C8)},
        {"left_hand_matrix", MatrixState(p->mf_9E0)}, {"shield_matrix", MatrixState(p->shieldMf)},
        {"previous_waist", Vec(p->unk_A88)}, {"feet", {Vec(p->actor.shape.feetPos[0]), Vec(p->actor.shape.feetPos[1])}},
        {"floor_pitch", p->floorPitch}, {"floor_pitch_alt", p->floorPitchAlt}, {"floor_property", p->floorProperty},
        {"previous_floor_type", p->prevFloorType}, {"floor_timer", p->floorTypeTimer},
        {"ledge_height", Float(p->yDistToLedge)}, {"wall_distance", Float(p->distToInteractWall)},
        {"ledge_type", p->ledgeClimbType}, {"ledge_timer", p->ledgeClimbDelayTimer},
        {"stick_history_index", p->controlStickDataIndex}, {"stick_directions", p->controlStickDirections},
        {"stick_spin_angles", p->controlStickSpinAngles}, {"previous_stick_angle", p->prevControlStickAngle},
        {"previous_stick_magnitude", Float(p->prevControlStickMagnitude)},
        {"textbox_cooldown", p->textboxBtnCooldownTimer}, {"pushed_speed", Float(p->pushedSpeed)},
        {"pushed_yaw", p->pushedYaw}, {"sword_quads", {QuadState(p->meleeWeaponQuads[0], play), QuadState(p->meleeWeaponQuads[1], play)}},
        {"shield_quad", QuadState(p->shieldQuad, play)}};
    state["cylinder"] = ColliderState(p->cylinder.base, play);
    state["cylinder"]["element"] = ColliderElement(p->cylinder.info);
    state["cylinder"]["position"] = Rot(p->cylinder.dim.pos);
    state["cylinder"]["height"] = p->cylinder.dim.height;
    state["cylinder"]["radius"] = p->cylinder.dim.radius;
    state["cylinder"]["y_shift"] = p->cylinder.dim.yShift;
    state["joints"] = json::array(); state["morph_joints"] = json::array();
    state["upper_joints"] = json::array(); state["upper_morph_joints"] = json::array();
    for (int i = 0; i < PLAYER_LIMB_BUF_COUNT; ++i) {
        state["joints"].push_back(Rot(p->jointTable[i]));
        state["morph_joints"].push_back(Rot(p->morphTable[i]));
        state["upper_joints"].push_back(Rot(p->upperJointTable[i]));
        state["upper_morph_joints"].push_back(Rot(p->upperMorphTable[i]));
    }
    state["signs"] = json::array();
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_PROP].head; a; a = a->next) {
        if (a->id != ACTOR_EN_KANBAN || a->init || !a->update) continue;
        auto* sign = reinterpret_cast<EnKanban*>(a);
        json target = {{"identity", ActorId(a)}, {"position", Vec(a->world.pos)},
            {"action", sign->actionState}, {"parts", sign->partFlags}, {"invincibility", sign->invincibilityTimer},
            {"cut_type", sign->cutType}, {"piece_type", sign->pieceType}, {"damage", a->colChkInfo.damage}};
        // Fragments do not initialize or register the sign cylinder.
        if (a->params != ENKANBAN_PIECE) {
            target["collider"] = ColliderState(sign->collider.base, play);
            target["element"] = ColliderElement(sign->collider.info);
        }
        state["signs"].push_back(std::move(target));
    }
    return state;
}
json CameraState(Camera* cam) {
    if (!cam) return nullptr;
    return {{"eye", Vec(cam->eye)}, {"at", Vec(cam->at)}, {"up", Vec(cam->up)},
            {"eye_next", Vec(cam->eyeNext)}, {"fov", Float(cam->fov)}, {"distance", Float(cam->dist)},
            {"xz_speed", Float(cam->xzSpeed)}, {"speed_ratio", Float(cam->speedRatio)},
            {"pos_offset", Vec(cam->posOffset)}, {"player_delta", Vec(cam->playerPosDelta)},
            {"input_direction", Rot(cam->inputDir)}, {"direction", Rot(cam->camDir)},
            {"mode", cam->mode}, {"setting", cam->setting}, {"status", cam->status},
            {"anim_state", cam->animState}, {"timer", cam->timer}, {"target", ActorId(cam->target)},
            {"at_lerp", Float(cam->atLERPStepScale)}, {"yaw_rate_inv", Float(cam->yawUpdateRateInv)},
            {"pitch_rate_inv", Float(cam->pitchUpdateRateInv)}, {"r_rate_inv", Float(cam->rUpdateRateInv)},
            {"fov_rate", Float(cam->fovUpdateRate)}, {"xz_offset_rate", Float(cam->xzOffsetUpdateRate)},
            {"y_offset_rate", Float(cam->yOffsetUpdateRate)}};
}
json DrawState(PlayState* play) {
    NativeSimHudObservation h = {};
    NativeSimMessageObservation m = {};
    Interface_GetNativeSimHudObservation(play, &h);
    Message_GetNativeSimObservation(play, &m);
    json hud, message;
    // Explicit fields only: never serialize POD padding, pointers, or resource text.
#define HUD_FIELD(name) hud[#name] = h.name
    HUD_FIELD(main_next_second); HUD_FIELD(main_state_timer);
    HUD_FIELD(sub_next_second); HUD_FIELD(sub_state_timer);
    HUD_FIELD(digits); HUD_FIELD(timer_x); HUD_FIELD(timer_y);
    HUD_FIELD(env_hazard); HUD_FIELD(env_hazard_active);
    HUD_FIELD(game_mode); HUD_FIELD(no_ui); HUD_FIELD(pause_state); HUD_FIELD(pause_debug_state);
    HUD_FIELD(game_over_state); HUD_FIELD(message_mode); HUD_FIELD(message_length);
    HUD_FIELD(player_state_flags1); HUD_FIELD(player_state_flags2); HUD_FIELD(player_state_flags3);
    HUD_FIELD(player_cs_action); HUD_FIELD(player_unk_6ad); HUD_FIELD(player_item_action); HUD_FIELD(magic_state);
    HUD_FIELD(transition_trigger); HUD_FIELD(transition_mode); HUD_FIELD(cutscene_state);
    HUD_FIELD(in_cutscene_mode); HUD_FIELD(minigame_state); HUD_FIELD(shooting_gallery_status);
    HUD_FIELD(scene); HUD_FIELD(bowling_switch_38); HUD_FIELD(timer_gate_open); HUD_FIELD(countdown_length_gate_open);
    HUD_FIELD(draw_debug_mode); HUD_FIELD(draw_world_enabled); HUD_FIELD(draw_overlay_enabled);
    HUD_FIELD(pause_menu_mode); HUD_FIELD(transition_unknown_state);
    HUD_FIELD(c_up_timer); HUD_FIELD(c_up_invisible); HUD_FIELD(counter_digits);
    HUD_FIELD(do_action_state); HUD_FIELD(do_action_current); HUD_FIELD(do_action_next);
    hud["do_action_rotation"] = Float(h.do_action_rotation);
    HUD_FIELD(navi_calling); HUD_FIELD(a_alpha); HUD_FIELD(b_alpha); HUD_FIELD(health_alpha);
    HUD_FIELD(magic_alpha); HUD_FIELD(screen_fill_alpha);
    hud["health_capacity"] = gSaveContext.healthCapacity;
#undef HUD_FIELD
    json hudPaint;
#define HUD_PAINT_FIELD(name) hudPaint[#name] = h.paint.name
    HUD_PAINT_FIELD(observed); HUD_PAINT_FIELD(draw_frame); HUD_PAINT_FIELD(clock_count);
    HUD_PAINT_FIELD(digit_count); HUD_PAINT_FIELD(timer_id); HUD_PAINT_FIELD(clock_x); HUD_PAINT_FIELD(clock_y);
    HUD_PAINT_FIELD(clock_width); HUD_PAINT_FIELD(clock_height); HUD_PAINT_FIELD(clock_s); HUD_PAINT_FIELD(clock_t);
    HUD_PAINT_FIELD(digit_values); HUD_PAINT_FIELD(digit_x); HUD_PAINT_FIELD(digit_y);
    HUD_PAINT_FIELD(digit_width); HUD_PAINT_FIELD(digit_height); HUD_PAINT_FIELD(digit_s); HUD_PAINT_FIELD(digit_t);
    HUD_PAINT_FIELD(digit_r); HUD_PAINT_FIELD(digit_g); HUD_PAINT_FIELD(digit_b); HUD_PAINT_FIELD(digit_a);
#undef HUD_PAINT_FIELD
    hud["paint"] = std::move(hudPaint);
#define MESSAGE_FIELD(name) message[#name] = m.name
    MESSAGE_FIELD(textId); MESSAGE_FIELD(choiceTextId); MESSAGE_FIELD(msgLength); MESSAGE_FIELD(msgMode);
    MESSAGE_FIELD(derivedState); MESSAGE_FIELD(textBoxProperties); MESSAGE_FIELD(textBoxType); MESSAGE_FIELD(textBoxPos);
    MESSAGE_FIELD(msgBufPos); MESSAGE_FIELD(textDrawPos); MESSAGE_FIELD(decodedTextLen); MESSAGE_FIELD(textUnskippable);
    MESSAGE_FIELD(textDelay); MESSAGE_FIELD(textDelayTimer); MESSAGE_FIELD(stateTimer); MESSAGE_FIELD(textboxEndType);
    MESSAGE_FIELD(choiceIndex); MESSAGE_FIELD(choiceNum); MESSAGE_FIELD(textPosX); MESSAGE_FIELD(textPosY);
    MESSAGE_FIELD(textColorR); MESSAGE_FIELD(textColorG); MESSAGE_FIELD(textColorB); MESSAGE_FIELD(textColorAlpha);
    MESSAGE_FIELD(textboxColorRed); MESSAGE_FIELD(textboxColorGreen); MESSAGE_FIELD(textboxColorBlue);
    MESSAGE_FIELD(textboxColorAlphaCurrent); MESSAGE_FIELD(textboxColorAlphaTarget);
    MESSAGE_FIELD(ocarinaMode); MESSAGE_FIELD(ocarinaAction); MESSAGE_FIELD(hasTalkActor);
    MESSAGE_FIELD(startFrameCount); MESSAGE_FIELD(textboxSkipped); MESSAGE_FIELD(nextTextId); MESSAGE_FIELD(textBoxNum);
    MESSAGE_FIELD(textFade); MESSAGE_FIELD(textIsCredits); MESSAGE_FIELD(messageHasSetSfx); MESSAGE_FIELD(lastPlayedSong);
    MESSAGE_FIELD(lastLanguage); MESSAGE_FIELD(displayAsEnglish); MESSAGE_FIELD(language);
    MESSAGE_FIELD(textboxX); MESSAGE_FIELD(textboxY); MESSAGE_FIELD(textboxWidth); MESSAGE_FIELD(textboxHeight);
    MESSAGE_FIELD(textboxTexWidth); MESSAGE_FIELD(textboxTexHeight); MESSAGE_FIELD(textboxXTarget); MESSAGE_FIELD(textboxYTarget);
    MESSAGE_FIELD(textboxWidthTarget); MESSAGE_FIELD(textboxHeightTarget);
    MESSAGE_FIELD(textboxTexWidthTarget); MESSAGE_FIELD(textboxTexHeightTarget);
    MESSAGE_FIELD(textboxEndX); MESSAGE_FIELD(textboxEndY); MESSAGE_FIELD(textInitX); MESSAGE_FIELD(textInitY);
    MESSAGE_FIELD(textLineSpacing); MESSAGE_FIELD(textCharScale); MESSAGE_FIELD(yreg15); MESSAGE_FIELD(yreg31);
    MESSAGE_FIELD(actionState); MESSAGE_FIELD(actionCurrent); MESSAGE_FIELD(actionTarget);
    MESSAGE_FIELD(actionOverride); MESSAGE_FIELD(actionOverrideId);
    message["actionRotation"] = Float(m.actionRotation);
    MESSAGE_FIELD(hudVisibilityMode); MESSAGE_FIELD(prevHudVisibilityMode);
    MESSAGE_FIELD(rawBufferValid); MESSAGE_FIELD(decodedBufferValid);
    message["rawBufferFingerprint"] = Hex(m.rawBufferFingerprint, 16);
    message["decodedBufferFingerprint"] = Hex(m.decodedBufferFingerprint, 16);
#undef MESSAGE_FIELD
    json paint;
#define PAINT_FIELD(name) paint[#name] = m.paint.name
    PAINT_FIELD(observed); PAINT_FIELD(entryMode); PAINT_FIELD(entryTextDrawPos); PAINT_FIELD(entryEndType);
    PAINT_FIELD(entryIconBranch); PAINT_FIELD(asciiPathComplete); PAINT_FIELD(displayListLinked);
    PAINT_FIELD(glyphCount); PAINT_FIELD(iconCount); PAINT_FIELD(iconType); PAINT_FIELD(iconX); PAINT_FIELD(iconY);
    PAINT_FIELD(drawFrame);
    paint["glyphFingerprint"] = Hex(m.paint.glyphFingerprint, 16);
    paint["iconFingerprint"] = Hex(m.paint.iconFingerprint, 16);
#undef PAINT_FIELD
    message["paint"] = std::move(paint);
    return {{"schema", 1}, {"hud", std::move(hud)}, {"message", std::move(message)},
            {"settings", {{"text_speed", CVarGetInteger(CVAR_ENHANCEMENT("TextSpeed"), 1)},
                          {"slow_text_speed", CVarGetInteger(CVAR_ENHANCEMENT("SlowTextSpeed"),
                                                           CVarGetInteger(CVAR_ENHANCEMENT("TextSpeed"), 1))},
                          {"skip_text", CVarGetInteger(CVAR_ENHANCEMENT("SkipText"), 0)},
                          {"text_spacing", CVarGetInteger(CVAR_ENHANCEMENT("TextSpacing"), 6)}}}};
}
json State(PlayState* play) {
    json rng = json::object();
    for (const auto& [name, stream] : streams)
        rng[name] = {{"calls", stream.calls}, {"draw_calls", stream.drawCalls},
                     {"state", stream.state}, {"order", Hex(stream.order, 16)}};
    json state = {{"schema", 1}, {"tick", tick}, {"time_q", tick * 6},
                  {"rng", rng}, {"audio", {{"blocks", audioBlocks}, {"samples", audioSamples},
                   {"clock_calls", audioClockCalls}, {"random", gAudioContext.audioRandom},
                   {"task_count", gAudioContext.totalTaskCnt}}}};
    state["audio"]["reset_status"] = static_cast<uint8_t>(gAudioContext.resetStatus);
    state["audio"]["reset_timer"] = static_cast<uint32_t>(gAudioContext.resetTimer);
    state["audio"]["command_write_position"] = gAudioContext.cmdWrPos;
    state["audio"]["command_read_position"] = gAudioContext.cmdRdPos;
    state["audio"]["memory_notes"] = json::array();
    for (const auto& note : sOcarinaSongNotes[OCARINA_SONG_MEMORY_GAME]) {
        state["audio"]["memory_notes"].push_back({{"pitch", note.pitch}, {"length", note.length},
            {"volume", note.volume}, {"vibrato", note.vibrato}, {"bend", note.bend}, {"b_flat_4", note.bFlat4Flag}});
    }
    state["audio"]["sequence_players"] = json::array();
    for (const auto& sequencePlayer : gAudioContext.seqPlayers) {
        state["audio"]["sequence_players"].push_back({
            {"enabled", static_cast<bool>(sequencePlayer.enabled)},
            {"finished", static_cast<bool>(sequencePlayer.finished)},
            {"muted", static_cast<bool>(sequencePlayer.muted)}, {"state", sequencePlayer.state},
            {"sequence", sequencePlayer.seqId}, {"tempo", sequencePlayer.tempo},
            {"tempo_accumulator", sequencePlayer.tempoAcc}, {"delay", sequencePlayer.delay},
            {"fade_timer", sequencePlayer.fadeTimer}, {"script_counter", sequencePlayer.scriptCounter},
            {"skip_ticks", sequencePlayer.skipTicks}, {"fade_volume", Float(sequencePlayer.fadeVolume)},
            {"volume", Float(sequencePlayer.volume)}, {"script_io", sequencePlayer.soundScriptIO}});
    }
    if (!play) return state;
    // Opt-in extension keeps the complete Pass 2 snapshot/trace contract unchanged.
    if (fixture.value("observe_draw_state", false)) state["draw_state"] = DrawState(play);
    state["global"] = {{"scene", play->sceneNum}, {"room", play->roomCtx.curRoom.num},
        {"entrance", gSaveContext.entranceIndex}, {"scene_epoch", sceneEpoch}, {"gameplay_frames", play->gameplayFrames},
        {"state_frames", play->state.frames}, {"day_time", gSaveContext.dayTime},
        {"skybox_time", gSaveContext.skyboxTime}, {"pause_state", play->pauseCtx.state},
        {"cutscene_state", play->csCtx.state}, {"cutscene_frame", play->csCtx.frames},
        {"transition_mode", play->transitionMode}, {"transition_trigger", play->transitionTrigger},
        {"update_rate", R_UPDATE_RATE}, {"health", gSaveContext.health}, {"magic", gSaveContext.magic},
        {"rupees", gSaveContext.rupees}, {"timer_state", gSaveContext.timerState},
        {"timer_seconds", gSaveContext.timerSeconds}, {"subtimer_state", gSaveContext.subTimerState},
        {"subtimer_seconds", gSaveContext.subTimerSeconds}, {"message_mode", play->msgCtx.msgMode},
        {"message_timer", play->msgCtx.stateTimer}, {"text_id", play->msgCtx.textId},
        {"text_draw_pos", play->msgCtx.textDrawPos}, {"ocarina_mode", play->msgCtx.ocarinaMode},
        {"switch_flags", play->actorCtx.flags.swch}, {"temp_switch_flags", play->actorCtx.flags.tempSwch},
        {"chest_flags", play->actorCtx.flags.chest}, {"clear_flags", play->actorCtx.flags.clear}};
    state["global"]["event_flags"] = gSaveContext.eventChkInf;
    state["global"]["event_info"] = gSaveContext.eventInf;
    state["global"]["inventory_items"] = gSaveContext.inventory.items;
    state["global"]["ammo"] = gSaveContext.inventory.ammo;
    auto& in = play->state.input[0];
    state["input"] = {{"held", in.cur.button}, {"pressed", in.press.button}, {"released", in.rel.button},
        {"stick_x", in.cur.stick_x}, {"stick_y", in.cur.stick_y},
        {"right_stick_x", in.cur.right_stick_x}, {"right_stick_y", in.cur.right_stick_y},
        {"previous_buttons", in.prev.button}, {"error", in.cur.err_no}};
    state["controllers"] = json::array();
    for (auto& port : play->state.input) {
        state["controllers"].push_back({{"held", port.cur.button}, {"pressed", port.press.button},
            {"released", port.rel.button}, {"stick_x", port.cur.stick_x}, {"stick_y", port.cur.stick_y},
            {"right_stick_x", port.cur.right_stick_x}, {"right_stick_y", port.cur.right_stick_y},
            {"error", port.cur.err_no}});
    }
    state["player"] = GET_PLAYER(play) ? PlayerState(GET_PLAYER(play)) : json(nullptr);
    if (fixture.value("observe_player_state", false)) state["player_detail"] = PlayerDetail(play);
    state["camera"] = CameraState(play->cameraPtrs[play->activeCamera]);
    state["collision"] = {{"at_count", play->colChkCtx.colATCount},
        {"ac_count", play->colChkCtx.colACCount}, {"oc_count", play->colChkCtx.colOCCount}};
    state["actors"] = json::array();
    for (int category = 0; category < ACTORCAT_MAX; ++category) {
        int order = 0;
        for (Actor* actor = play->actorCtx.actorLists[category].head; actor; actor = actor->next) {
            auto a = ActorState(actor);
            a["list_order"] = order++;
            // Base fields are explicit coverage; per-family action/timer serializers come with fixtures.
            a["coverage"] = "base_actor";
            state["actors"].push_back(std::move(a));
        }
    }
    return state;
}

// These bytes never enter a portable hash or a semantic golden. They detect any
// same-process write, including scratch fields and pointer aliases omitted from
// portable snapshots. No live state/context is swapped or restored.
json playerPoseAdmission = json::object();
std::vector<unsigned char> PresentationLiveBytes(PlayState* play, bool playerPose) {
    std::vector<unsigned char> bytes;
    auto append = [&](const void* data, size_t size) {
        const auto* first = static_cast<const unsigned char*>(data);
        bytes.insert(bytes.end(), first, first + size);
    };
    append(play, sizeof(*play)); // includes View, Font, all inputs, interface and message scratch
    append(&gSaveContext, sizeof(gSaveContext));
    append(gGameInfo, sizeof(*gGameInfo));
    append(gSegments, sizeof(gSegments));
    append(&gPadMgr, sizeof(gPadMgr));
    if (play->interfaceCtx.doActionSegment) append(play->interfaceCtx.doActionSegment, 3 * sizeof(char*));
    NativeSimHudObservation hud{};
    Interface_GetNativeSimHudObservation(play, &hud);
    append(&hud, sizeof(hud));
    NativeSimMessageObservation message{};
    Message_GetNativeSimObservation(play, &message);
    append(&message, sizeof(message));
    unsigned char statics[256]{};
    const auto size = Message_CopyPresentationStatics(statics);
    append(statics, size);
    if (playerPose) {
        append(GET_PLAYER(play), sizeof(Player));
        for (int category = 0; category < ACTORCAT_MAX; ++category) {
            for (Actor* actor = play->actorCtx.actorLists[category].head; actor; actor = actor->next) {
                append(actor, actor->id == ACTOR_EN_KANBAN ? sizeof(EnKanban) : sizeof(Actor));
            }
        }
        append(&sEffectContext, sizeof(sEffectContext));
        append(&sEffectSsInfo, sizeof(sEffectSsInfo));
        if (sEffectSsInfo.table) append(sEffectSsInfo.table, sEffectSsInfo.tableSize * sizeof(EffectSs));
        append(statics, Player_CopyPoseStatics(statics));
        for (const auto& entry : *GameInteractor::Instance->GetHookData<GameInteractor::OnVanillaBehavior>()) {
            append(&entry.first, sizeof(entry.first));
            append(&entry.second.calls, sizeof(entry.second.calls));
        }
    }
    return bytes;
}
void WritePurity(const char* status, const std::string& failure = "") {
    json result = {{"schema", 1}, {"status", status}, {"fixture", fixture},
        {"extra_calls", 2}, {"packet_bytes_checked", true},
        {"coverage", purityCoverage}, {"admission_negatives", admissionCoverage},
        {"player_admission", playerPoseAdmission},
        {"first_failure", failure}, {"tick", tick}, {"phase", phase},
        {"negative_control", purityNegativeControl},
        {"comparison", "same-process live bytes plus complete semantic state and event sequence; ordered Gfx words and paint"}};
    std::ofstream file(output / "purity.json");
    file << result.dump(2) << '\n';
    if (!file) Fail("purity receipt write failed");
}
void CheckAdmissionNegatives(const std::string& name, PlayState* play) {
    if (admissionCoverage.contains(name)) return;
    auto copy = std::make_unique<PlayState>();
    if (name == "player") {
        Player player;
        int tests = 0;
        auto check = [&](const char* label, auto change) {
            std::memcpy(copy.get(), play, sizeof(*play));
            std::memcpy(&player, GET_PLAYER(play), sizeof(player));
            copy->actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
            player.skelAnime.jointTable = reinterpret_cast<Vec3s*>(ALIGN16(reinterpret_cast<uintptr_t>(player.jointTable)));
            std::memcpy(player.skelAnime.jointTable, GET_PLAYER(play)->skelAnime.jointTable,
                        player.skelAnime.limbCount * sizeof(Vec3s) + sizeof(s16));
            if (!Player_IsPoseProfileAdmitted(copy.get(), &player))
                Fail("Player negative-admission baseline copy is not admitted");
            change(*copy, player);
            if (Player_IsPoseProfileAdmitted(copy.get(), &player)) {
                WritePurity("fail", std::string("player admission: ") + label);
                Fail("unsupported Player pose admitted");
            }
            ++tests;
        };
        check("pause", [](auto& p, auto&) { p.pauseCtx.state = 1; });
        check("transition", [](auto& p, auto&) { p.transitionTrigger = TRANS_TRIGGER_START; });
        check("reflection", [](auto&, auto& p) { p.stateFlags2 |= PLAYER_STATE2_REFLECTION; });
        check("first person", [](auto&, auto& p) { p.unk_6AD = 2; });
        check("crawl", [](auto&, auto& p) { p.stateFlags2 |= PLAYER_STATE2_CRAWLING; });
        check("carry", [](auto&, auto& p) { p.heldActor = &p.actor; });
        check("get item", [](auto&, auto& p) { p.stateFlags1 |= PLAYER_STATE1_GETTING_ITEM; });
        check("weapon", [](auto&, auto& p) { p.heldItemAction = PLAYER_IA_HAMMER; });
        check("shield", [](auto&, auto& p) { p.currentShield = PLAYER_SHIELD_HYLIAN; });
        check("dynamic floor", [](auto&, auto& p) { p.actor.floorBgId = 0; });
        check("airborne", [](auto&, auto& p) { p.actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND; });
        check("frozen", [](auto&, auto& p) { p.stateFlags2 |= PLAYER_STATE2_FROZEN; });
        check("draw disabled", [](auto&, auto& p) { p.stateFlags2 |= PLAYER_STATE2_DISABLE_DRAW; });
        // Test registry entry is never executed; this proves unknown limb hooks
        // reject before callbacks, without loading an external modification.
        using Hooks = GameInteractor::RegisteredGameHooks<GameInteractor::OnVanillaBehavior>;
        auto& limbHooks = Hooks::functionsForID[VB_PLAYER_OVERRIDE_LIMB_DRAW];
        const uint32_t probeId = UINT32_MAX;
        if (limbHooks.count(probeId)) Fail("admission probe ID already registered");
        limbHooks.emplace(probeId, [](GIVanillaBehavior, bool*, va_list) {});
        const bool rejected = !Player_IsPoseProfileAdmitted(play, GET_PLAYER(play));
        limbHooks.erase(probeId);
        if (!rejected) Fail("custom limb hook admitted");
        admissionCoverage[name] = tests + 1;
        return;
    }
    int tests = 0;
    auto check = [&](const char* label, auto change) {
        std::memcpy(copy.get(), play, sizeof(*play));
        change(*copy);
        int admitted = name == "countdown" ? Interface_IsCountdownProfileAdmitted(copy.get()) :
                                             Message_IsPlainTextProfileAdmitted(copy.get());
        if (admitted) { WritePurity("fail", name + " admission: " + label); Fail("unsupported presentation admitted"); }
        ++tests;
    };
    check("pause", [](auto& p) { p.pauseCtx.state = 1; });
    check("freeze", [](auto& p) { p.actorCtx.freezeFlashTimer = 2; });
    check("frame advance", [](auto& p) { p.frameAdvCtx.enabled = 1; });
    check("scene", [](auto& p) { p.sceneNum = SCENE_BOMBCHU_BOWLING_ALLEY; });
    check("transition", [](auto& p) { p.transitionTrigger = TRANS_TRIGGER_START; });
    if (name == "message") {
        check("fade fixture", [](auto& p) { p.msgCtx.textId = 0x305F; });
        check("talker", [](auto& p) { p.msgCtx.talkActor = GET_PLAYER(&p) ? &GET_PLAYER(&p)->actor : (Actor*)&p; });
        check("choice", [](auto& p) { p.msgCtx.choiceNum = 2; });
        check("type", [](auto& p) { p.msgCtx.textBoxType = TEXTBOX_TYPE_OCARINA; });
        check("quicktext control", [](auto& p) { p.msgCtx.font.msgBuf[0] = 0x08; });
        check("length", [](auto& p) { p.msgCtx.msgLength = 65; });
        check("end type", [](auto& p) { p.msgCtx.textboxEndType = TEXTBOX_ENDTYPE_FADING; });
        check("mode", [](auto& p) { p.msgCtx.msgMode = MSGMODE_TEXT_CONTINUING; });
    } else {
        check("message gate", [](auto& p) { p.msgCtx.msgMode = MSGMODE_TEXT_DISPLAYING; });
        check("shooting gallery", [](auto& p) { p.shootingGalleryStatus = 1; });
    }
    admissionCoverage[name] = tests;
}
void WriteSnapshot() {
    snapshots << State(gPlayState).dump() << '\n';
    if (!snapshots) Fail("snapshot write failed");
}
void ApplySetup() {
    if (fixture.contains("message_text_id")) {
        Message_StartTextbox(gPlayState, fixture.at("message_text_id").get<uint16_t>(), nullptr);
    }
    if (fixture.value("spawn_ice_keese", false)) {
        // Fixture construction through the real spawn path, with its scene object
        // already loaded. The actor's unchanged limb draw consumes gameplay RNG.
        int objectIndex = Object_GetIndex(&gPlayState->objectCtx, OBJECT_FIREFLY);
        if (objectIndex < 0 || !Object_IsLoaded(&gPlayState->objectCtx, objectIndex))
            Fail("ice Keese fixture requires the loaded Firefly object bank");
        Camera* camera = GET_ACTIVE_CAM(gPlayState);
        if (!camera || !Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_FIREFLY,
                camera->at.x, camera->at.y + 30.0f, camera->at.z, 0, 0, 0, 4))
            Fail("ice Keese fixture spawn failed");
    }
    if (fixture.contains("hud_timer_seconds")) {
        // Use the real HUD state-machine entry. Interface_Draw owns subsequent
        // preview, movement and per-second countdown; the runner never ticks it.
        Interface_SetTimer(fixture.at("hud_timer_seconds").get<int16_t>());
    }
    if (fixture.contains("ocarina_memory_round")) {
        // This is the real three-note memory-game initialization, including its
        // audio RNG coupling and nonrepeat rule, not synthetic random draws.
        AudioOcarina_MemoryGameInit(fixture.at("ocarina_memory_round").get<uint8_t>());
    }
    if (fixture.contains("initial_player")) {
        auto& init = fixture.at("initial_player");
        Player* player = GET_PLAYER(gPlayState);
        if (init.contains("pos")) {
            auto& pos = init.at("pos");
            player->actor.world.pos = {pos.at(0).get<float>(), pos.at(1).get<float>(), pos.at(2).get<float>()};
            player->actor.prevPos = player->actor.world.pos;
        }
        if (init.contains("yaw")) {
            player->yaw = init.at("yaw").get<int16_t>();
            player->actor.world.rot.y = player->actor.shape.rot.y = player->yaw;
        }
    }
    if (fixture.value("spawn_cuttable_sign", false)) {
        int objectIndex = Object_GetIndex(&gPlayState->objectCtx, OBJECT_KANBAN);
        if (objectIndex < 0 || !Object_IsLoaded(&gPlayState->objectCtx, objectIndex))
            Fail("cuttable sign fixture requires the loaded Kanban object bank");
        Player* p = GET_PLAYER(gPlayState);
        const float distance = 45.0f;
        if (!Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_KANBAN,
                p->actor.world.pos.x + Math_SinS(p->yaw) * distance, p->actor.world.pos.y,
                p->actor.world.pos.z + Math_CosS(p->yaw) * distance, 0, p->yaw + 0x8000, 0, 0))
            Fail("cuttable sign fixture spawn failed");
    }
}
} // namespace

const json& NativeSimTest_GetFixture() { return fixture; }

extern "C" void* NativeSimTest_Present(const char* helper, PlayState* play, const void* packet, size_t packetSize,
                                       void* outputBuffer, void* paint, size_t paintSize, int visible,
                                       NativeSimPresentationHelper emit) {
    if (!verifyPresentationPurity) return emit(packet, outputBuffer, paint);
    CheckAdmissionNegatives(helper, play);
    const auto beforeBytes = PresentationLiveBytes(play, std::strcmp(helper, "player") == 0);
    const auto beforeState = State(play);
    const auto beforeDrawState = DrawState(play); // also required when fixture observation is off
    const auto beforeSequence = sequence;
    // Both packet builders initialize their entire stack object. These bytes
    // stay process-local, just like the live-state comparison; no pointer or
    // padding is admitted into a portable hash or canonical reference.
    const std::vector<unsigned char> beforePacket(static_cast<const unsigned char*>(packet),
                                                  static_cast<const unsigned char*>(packet) + packetSize);
    const std::vector<unsigned char> initialPaint(static_cast<unsigned char*>(paint),
                                                 static_cast<unsigned char*>(paint) + paintSize);
    auto fail = [&](const std::string& reason) {
        WritePurity("fail", std::string(helper) + ": " + reason);
        Fail(std::string("presentation purity: ") + helper + ": " + reason);
    };
    auto checkLive = [&]() {
        if (std::memcmp(packet, beforePacket.data(), packetSize)) fail("packet mutated");
        if (PresentationLiveBytes(play, std::strcmp(helper, "player") == 0) != beforeBytes || State(play) != beforeState ||
            DrawState(play) != beforeDrawState || sequence != beforeSequence) fail("live state mutated");
    };
    auto* begin = static_cast<Gfx*>(outputBuffer);
    auto* end = static_cast<Gfx*>(emit(packet, outputBuffer, paint));
    checkLive();
    const auto count = end - begin;
    // Both bounded packet schemas emit fewer than 4096 commands. Extra buffers
    // have their own storage; they are never submitted or registered for replay.
    if (count < 0 || count >= 4096) fail("emission exceeds packet command bound");
    for (int repetition = 0; repetition < 2; ++repetition) {
        std::vector<Gfx> scratch(4096);
        auto scratchPaint = initialPaint;
        auto* scratchEnd = static_cast<Gfx*>(emit(packet, scratch.data(), scratchPaint.data()));
        // Explicit test-only detector control exits gracefully, without rendering
        // or another transaction. It is never enabled by an ordinary fixture.
        if (purityNegativeControl && repetition == 0 &&
            (!fixture.value("observe_player_state", false) || std::strcmp(helper, "player") == 0)) {
            if (std::strcmp(helper, "player") == 0) ++GET_PLAYER(play)->unk_845;
            else ++play->msgCtx.stateTimer;
        }
        checkLive();
        if (scratchEnd - scratch.data() != count || std::memcmp(paint, scratchPaint.data(), paintSize))
            fail("paint or command count changed");
        for (ptrdiff_t i = 0; i < count; ++i)
            if (begin[i].words.w0 != scratch[i].words.w0 || begin[i].words.w1 != scratch[i].words.w1)
                fail("ordered command emission changed at " + std::to_string(i));
    }
    NativeSimRecordPresentationCoverage(purityCoverage, helper, measuring, visible, static_cast<uint64_t>(count));
    return end;
}

extern "C" void NativeSimTest_PlayerPoseAdmission(PlayState* play, const char* rejection) {
    if (!verifyPresentationPurity || !measuring || !NativeSimTest_ObservePlayerState()) return;
    const std::string key = rejection ? rejection : "admitted";
    playerPoseAdmission[key] = playerPoseAdmission.value(key, 0u) + 1;
}

extern "C" int NativeSimTest_IsEnabled() { return enabled; }
extern "C" int NativeSimTest_IsMeasuring() { return enabled && measuring; }
extern "C" int NativeSimTest_ConfigInt(const char* key, int fallback) {
    return enabled ? fixture.value(key, fallback) : fallback;
}
extern "C" uint64_t NativeSimTest_TimeQ() { return tick * 6; }
extern "C" uint32_t NativeSimTest_Seed() { return fixture.value("seed", 1u); }
extern "C" int NativeSimTest_ObserveDrawState() {
    return enabled && fixture.value("observe_draw_state", false);
}
extern "C" int NativeSimTest_ObservePlayerState() {
    return enabled && fixture.value("observe_player_state", false);
}
extern "C" void NativeSimTest_PlayerSample(const char* site, PlayState* play) {
    if (!NativeSimTest_ObservePlayerState() || !play || !GET_PLAYER(play)) return;
    if (std::strcmp(site, "pose.end") == 0) ++playerPoseGeneration;
    if (!measuring) return;
    NativeSimTest_TraceJson({{"kind", "player_sample"}, {"site", site},
        {"player", PlayerState(GET_PLAYER(play))}, {"detail", PlayerDetail(play)},
        {"camera", CameraState(GET_ACTIVE_CAM(play))},
        {"input", {{"pressed", play->state.input[0].press.button}, {"held", play->state.input[0].cur.button}}}});
}
extern "C" void NativeSimTest_PlayerActorSample(const char* site, PlayState* play, Actor* actor) {
    if (!NativeSimTest_ObservePlayerState() || !actor) return;
    if (actor == &GET_PLAYER(play)->actor || actor->id == ACTOR_EN_KANBAN)
        NativeSimTest_PlayerSample(site, play);
}
extern "C" void NativeSimTest_PlayerRegistration(PlayState* play, const char* category, const void* collider, int index) {
    if (!NativeSimTest_ObservePlayerState() || !measuring) return;
    const auto& c = *static_cast<const Collider*>(collider);
    if (c.actor != &GET_PLAYER(play)->actor) return;
    NativeSimTest_TraceJson({{"kind", "player_registration"}, {"category", category}, {"index", index},
        {"collider", ColliderState(c, play)}});
}
extern "C" void NativeSimTest_PlayerContact(PlayState* play, const void* attack, const void* defense,
                                            uint32_t damageFlags, float x, float y, float z) {
    if (!NativeSimTest_ObservePlayerState()) return;
    const auto& at = *static_cast<const Collider*>(attack);
    const auto& ac = *static_cast<const Collider*>(defense);
    if (at.actor != &GET_PLAYER(play)->actor && ac.actor != &GET_PLAYER(play)->actor) return;
    ++playerContacts;
    NativeSimTest_TraceJson({{"kind", "player_contact"}, {"ordinal", playerContacts},
        {"attack", ColliderState(at, play)}, {"defense", ColliderState(ac, play)},
        {"damage_flags", damageFlags}, {"position", Vec(Vec3f{x, y, z})}});
}
extern "C" uint32_t NativeSimTest_AudioClock() {
    // Test-only counter clock. No host time or device occupancy enters this stream.
    return 0x12345678u + 781250u * static_cast<uint32_t>(audioBlocks) + audioClockCalls++;
}
void NativeSimTest_TraceJson(const json& record) {
    if (!enabled || !verbose) return;
    json event = record;
    event["schema"] = 1;
    event["sequence"] = sequence++;
    event["tick"] = tick;
    event["time_q"] = tick * 6;
    event["engine_frame"] = engineFrames;
    event["measuring"] = measuring;
    event["phase"] = phase;
    event["actor"] = ActorId(currentActor);
    // Logging initializes after argument validation. Retain any early events,
    // but do not expose replay file handles to startup's console logger.
    if (!trace.is_open()) {
        pendingTrace.push_back(event.dump());
        return;
    }
    trace << event.dump() << '\n';
    if (!trace) Fail("trace write failed");
}
extern "C" void NativeSimTest_Event(const char* kind, const char* site, uint32_t value) {
    if (!enabled) return;
    if (std::strcmp(kind, "rng-seed") == 0) {
        if (std::strcmp(site, "Rand_Seed") == 0) streams["gameplay"].state = value;
        else if (std::strcmp(site, "Rand_Seed_Variable") == 0) streams["variable"].state = value;
    } else if (std::strcmp(site, "ShipUtils::RandInit") == 0) {
        if (std::strcmp(kind, "rng-seed-low") == 0) streams["ship-utils"].state = value;
        else if (std::strcmp(kind, "rng-seed-high") == 0) streams["ship-utils-high"].state = value;
    } else if (std::strcmp(kind, "rng-state-high") == 0 && std::strcmp(site, "ShipUtils::next32") == 0) {
        // Companion state word, not an independently advanced generator.
        streams["ship-utils-high"].state = value;
    }
    static const char* authoritativeKinds[] = {
        "rng-seed", "rng-seed-low", "rng-seed-high", "rng-state-high", "rng-output", "rng-draw",
        "audio_block", "audio-sfx-request", "audio-sfx-queued", "audio-sequence-command", "ocarina-memory-note"
    };
    for (const char* authoritativeKind : authoritativeKinds) {
        if (std::strcmp(kind, authoritativeKind) != 0) continue;
        auto& events = streams["events"];
        ++events.calls;
        events.state = value;
        std::string entry = std::string(kind) + ":" + site + ":" + phase + ":" +
                            ActorId(currentActor).dump() + ":" + std::to_string(value);
        for (unsigned char c : entry) events.order = (events.order ^ c) * 1099511628211ull;
        break;
    }
    NativeSimTest_TraceJson({{"kind", kind}, {"site", site}, {"value", value}});
}
extern "C" void NativeSimTest_Rng(const char* stream, const char* site, uint32_t state) {
    if (!enabled) return;
    auto& rng = streams[stream];
    ++rng.calls;
    if (phase.rfind("draw", 0) == 0) ++rng.drawCalls;
    rng.state = state;
    // Rolling fingerprint retains draw order even when verbose output is disabled.
    std::string entry = std::string(site) + ":" + phase + ":" + ActorId(currentActor).dump() + ":" + std::to_string(state);
    for (unsigned char c : entry) rng.order = (rng.order ^ c) * 1099511628211ull;
    NativeSimTest_TraceJson({{"kind", "rng"}, {"stream", stream}, {"site", site}, {"ordinal", rng.calls}, {"state", state}});
    NativeSimTest_Event("rng-draw", site, state);
}
extern "C" void NativeSimTest_AudioBlock(int samples) {
    if (!enabled) return;
    ++audioBlocks;
    audioSamples += samples;
    NativeSimTest_Event("audio_block", "AudioMgr_CreateNextAudioBuffer", samples);
}
extern "C" void NativeSimTest_ActorSpawn(Actor* actor) {
    if (!enabled) return;
    actorIds[actor] = ++spawnOrdinal;
    NativeSimTest_TraceJson({{"kind", "spawn"}, {"identity", ActorId(actor)}, {"type", actor->id}});
}
extern "C" void NativeSimTest_ActorDestroy(Actor* actor) {
    if (!enabled) return;
    NativeSimTest_TraceJson({{"kind", "destroy"}, {"identity", ActorId(actor)}, {"type", actor->id}});
}
extern "C" void NativeSimTest_ActorFree(Actor* actor) {
    if (!enabled) return;
    NativeSimTest_TraceJson({{"kind", "free"}, {"identity", ActorId(actor)}});
    actorIds.erase(actor);
}
extern "C" void NativeSimTest_SceneInit() {
    if (!enabled) return;
    if (measuring) Fail("scene transitions are outside the schema-1 canonical fixture envelope");
    ++sceneEpoch;
    spawnOrdinal = 0;
    actorIds.clear();
    NativeSimTest_Phase("scene_init", nullptr);
}
extern "C" void NativeSimTest_ActorScope(Actor* actor) {
    if (!enabled) return;
    if (actor) actorScope.push_back(actor);
    else if (!actorScope.empty()) actorScope.pop_back();
    currentActor = actorScope.empty() ? nullptr : actorScope.back();
}
extern "C" void NativeSimTest_Phase(const char* next, PlayState* play) {
    if (!enabled) return;
    if (std::strcmp(next, "update_begin") == 0) ++updateCalls;
    if (std::strcmp(next, "draw_begin") == 0) ++drawCalls;
    if (verbose && play && measuring) {
        auto state = State(play);
        if (!previousPhase.is_null()) {
            auto diff = json::diff(previousPhase, state);
            if (!diff.empty()) NativeSimTest_TraceJson({{"kind", "phase_mutations"}, {"until", next}, {"changes", diff}});
        }
        previousPhase = std::move(state);
    }
    phase = next;
    if (!actorScope.empty()) Fail("unbalanced actor trace scope at phase boundary");
    currentActor = nullptr;
    NativeSimTest_Event("phase", next, 0);
}
extern "C" void NativeSimTest_Init(int argc, char** argv) {
    std::string fixturePath;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--native-sim-test") {
            enabled = true;
            if (++i == argc) Fail("--native-sim-test requires a fixture path");
            fixturePath = argv[i];
        } else if (arg == "--output" && i + 1 < argc) output = argv[++i];
        else if (arg == "--trace") verbose = true;
        else if (arg == "--verify-presentation-purity") verifyPresentationPurity = true;
        else if (arg == "--presentation-purity-negative-control") purityNegativeControl = true;
    }
    if ((verifyPresentationPurity || purityNegativeControl) && !enabled)
        Fail("presentation purity options require --native-sim-test");
    if (purityNegativeControl && !verifyPresentationPurity)
        Fail("negative control requires --verify-presentation-purity");
    if (!enabled) return;
    try {
        if (output.empty()) throw std::runtime_error("test mode requires --output");
        std::filesystem::create_directories(output);
        if (std::filesystem::exists(output / "result.json") || std::filesystem::exists(output / "snapshots.jsonl") ||
            std::filesystem::exists(output / "trace.jsonl"))
            throw std::runtime_error("test output already contains results; choose a fresh directory");
        std::ifstream file(fixturePath);
        if (!file) throw std::runtime_error("cannot open fixture");
        file >> fixture;
        if (!fixture.is_object()) throw std::runtime_error("fixture must be a JSON object");
        auto integer = [](const json& object, const char* key, int64_t minimum, int64_t maximum,
                          int64_t fallback, bool required = false) {
            auto item = object.find(key);
            if (item == object.end()) {
                if (required) throw std::runtime_error(std::string("fixture missing ") + key);
                return fallback;
            }
            if (!item->is_number_integer() ||
                (item->is_number_unsigned() && item->get<uint64_t>() > static_cast<uint64_t>(INT64_MAX)))
                throw std::runtime_error(std::string("fixture ") + key + " must be an integer");
            int64_t value = item->get<int64_t>();
            if (value < minimum || value > maximum)
                throw std::runtime_error(std::string("fixture ") + key + " is outside its supported range");
            return value;
        };
        integer(fixture, "schema", 1, 1, 1, true);
        integer(fixture, "rate_hz", 20, 20, 20);
        const auto& id = fixture.at("id");
        if (!id.is_string() || id.get<std::string>().empty()) throw std::runtime_error("fixture id must be nonempty text");
        integer(fixture, "ticks", 1, 100000, 0, true);
        integer(fixture, "setup_ticks", 1, 10000, 60);
        integer(fixture, "seed", 0, UINT32_MAX, 1);
        integer(fixture, "age", 0, 1, 1);
        // Play_Init selects one of four day/age entrance-layer entries.
        integer(fixture, "entrance", 0, ENTR_MAX - 4, ENTR_LINKS_HOUSE_CHILD_SPAWN);
        integer(fixture, "presentation_fps", 20, 360, 20);
        integer(fixture, "hud_timer_seconds", 1, 3599, 1);
        integer(fixture, "ocarina_memory_round", 0, 2, 0);
        integer(fixture, "message_text_id", 0, UINT16_MAX, 0);
        for (const char* key : {"observe_player_state", "spawn_cuttable_sign"})
            if (fixture.contains(key) && !fixture.at(key).is_boolean())
                throw std::runtime_error(std::string(key) + " must be a boolean");
        if (fixture.value("spawn_cuttable_sign", false) && !fixture.value("observe_player_state", false))
            throw std::runtime_error("cuttable sign recipe requires Player observation");
        if (fixture.contains("observe_draw_state") && !fixture.at("observe_draw_state").is_boolean())
            throw std::runtime_error("observe_draw_state must be a boolean");
        if (fixture.contains("spawn_ice_keese") && !fixture.at("spawn_ice_keese").is_boolean())
            throw std::runtime_error("spawn_ice_keese must be a boolean");
        if (fixture.contains("initial_player")) {
            const auto& player = fixture.at("initial_player");
            if (!player.is_object()) throw std::runtime_error("initial_player must be an object");
            integer(player, "yaw", INT16_MIN, INT16_MAX, 0);
            if (player.contains("pos")) {
                const auto& pos = player.at("pos");
                if (!pos.is_array() || pos.size() != 3)
                    throw std::runtime_error("initial_player pos must contain exactly three coordinates");
                for (const auto& coordinate : pos) {
                    if (!coordinate.is_number() || !std::isfinite(coordinate.get<double>()) ||
                        std::abs(coordinate.get<double>()) > 32767.0)
                        throw std::runtime_error("initial_player coordinate is outside the finite scene range");
                }
            }
        }
        NativeSimTest_ValidateInput();
    } catch (const std::exception& error) { Fail(error.what()); }
}
extern "C" void NativeSimTest_Configure() {
    if (!enabled) return;
    // Context::InitLogging has replaced the startup logger by this point.
    // Opening before that allowed stale console handles to corrupt JSON output.
    snapshots.open(output / "snapshots.jsonl");
    if (verbose) trace.open(output / "trace.jsonl");
    if (!snapshots || (verbose && !trace)) Fail("cannot open output files");
    for (const auto& event : pendingTrace) trace << event << '\n';
    pendingTrace.clear();
    if (verbose && !trace) Fail("buffered trace write failed");
    CVarSetInteger(CVAR_SETTING("InterpolationFPS"), NativeSimTest_ConfigInt("presentation_fps", 20));
    CVarSetInteger(CVAR_SETTING("MatchRefreshRate"), 0);
    CVarSetInteger(CVAR_VSYNC_ENABLED, 0);
    CVarSetInteger(CVAR_GENERAL("LetItSnow"), 0);
    CVarSetInteger(CVAR_CHEAT("TimeSync"), 0);
    CVarSetInteger(CVAR_SETTING("EnableMouse"), 0);
    CVarSetInteger(CVAR_SETTING("Mods.AlternateAssetsHotkey"), 0);
    CVarSetInteger(CVAR_CHEAT("SaveStatesEnabled"), 0);
    CVarSetInteger(CVAR_CHEAT("EasyFrameAdvance"), 0);
    CVarSetInteger(CVAR_REMOTE_CROWD_CONTROL("Enabled"), 0);
    CVarSetInteger(CVAR_REMOTE_SAIL("Enabled"), 0);
    CVarSetInteger(CVAR_REMOTE_ANCHOR("Enabled"), 0);
}
extern "C" void NativeSimTest_BeginFrame() {
    if (!enabled) return;
    ++engineFrames;
    updateCalls = drawCalls = 0;
    NativeSimTest_Phase("input_poll", nullptr);
}
extern "C" void NativeSimTest_EndFrame() {
    if (!enabled) return;
    NativeSimTest_Phase("transaction_end", gPlayState);
    if (!gPlayState || !GET_PLAYER(gPlayState)) Fail("fixture did not initialize Player");
    if (!measuring) {
        if (++setupFrames >= static_cast<uint64_t>(NativeSimTest_ConfigInt("setup_ticks", 60))) {
            if (R_UPDATE_RATE != 3) Fail("setup did not reach canonical world cadence");
            ApplySetup();
            measuring = true;
            previousPhase = nullptr;
            WriteSnapshot();
        }
        return;
    }
    if (R_UPDATE_RATE != 3 || updateCalls != 1 || drawCalls != 1)
        Fail("fixture left canonical cadence or did not execute exactly one update and CPU draw");
    ++tick;
    WriteSnapshot();
    if (tick == fixture.at("ticks").get<uint64_t>()) {
        snapshots.flush();
        if (verbose) trace.flush();
        if (verifyPresentationPurity) WritePurity("pass");
        json result = {{"schema", 1}, {"status", "pass"}, {"fixture_id", fixture.at("id")},
            {"ticks_completed", tick}, {"rate_hz", 20}, {"time_q", tick * 6},
            {"engine_frames", engineFrames}, {"setup_ticks", setupFrames},
            {"fixture", fixture},
            {"configuration", {{"interpolation_fps", CVarGetInteger(CVAR_SETTING("InterpolationFPS"), 20)},
                {"match_refresh_rate", CVarGetInteger(CVAR_SETTING("MatchRefreshRate"), 0)},
                {"mouse", CVarGetInteger(CVAR_SETTING("EnableMouse"), 0)},
                {"time_sync", CVarGetInteger(CVAR_CHEAT("TimeSync"), 0)}}},
            {"snapshot_phase", "after_graphics_audio_and_savestate_requests"},
            {"audio_policy", "synchronous_528_sample_blocks_legacy_grouping"},
            {"coverage", "semantic_v1_player_camera_base_actors_rng_audio_no_generic_actor_actions"}};
        auto temp = output / "result.pending.json";
        { std::ofstream file(temp); file << result.dump(2) << '\n'; if (!file) Fail("manifest write failed"); }
        std::filesystem::rename(temp, output / "result.json");
        std::exit(0);
    }
}
