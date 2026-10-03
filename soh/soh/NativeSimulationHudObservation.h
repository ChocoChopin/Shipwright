#ifndef NATIVE_SIMULATION_HUD_OBSERVATION_H
#define NATIVE_SIMULATION_HUD_OBSERVATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

// Last CPU Interface_Draw timer emissions; no GPU-completion claim. Positions,
// dimensions and texture steps are the actual Gfx_Texture* arguments.
typedef struct NativeSimHudPaintObservation {
    int32_t observed;
    uint32_t draw_frame;
    int32_t clock_count, digit_count, timer_id;
    int32_t clock_x, clock_y, clock_width, clock_height, clock_s, clock_t;
    int32_t digit_values[5], digit_x[5], digit_y[5];
    int32_t digit_width[5], digit_height[5], digit_s[5], digit_t[5];
    int32_t digit_r, digit_g, digit_b, digit_a;
} NativeSimHudPaintObservation;

// Diagnostic values only: no pointers, restoration API, or timer ownership change.
typedef struct NativeSimHudObservation {
    int16_t main_next_second;
    int16_t main_state_timer;
    int16_t sub_next_second;
    int16_t sub_state_timer;
    int16_t digits[5];
    int16_t timer_x[2];
    int16_t timer_y[2];
    int16_t env_hazard;
    int16_t env_hazard_active;

    // Raw inputs to the legacy HUD/countdown gates. timer_gate_open describes
    // the inner timer conditional, not proof that Play_Draw reached that block.
    int32_t game_mode;
    int32_t no_ui;
    int32_t pause_state;
    int32_t pause_debug_state;
    int32_t game_over_state;
    int32_t message_mode;
    int32_t message_length;
    uint32_t player_state_flags1;
    uint32_t player_state_flags2;
    uint32_t player_state_flags3;
    int32_t player_cs_action;
    int32_t player_unk_6ad;
    int32_t player_item_action;
    int32_t magic_state;
    int32_t transition_trigger;
    int32_t transition_mode;
    int32_t cutscene_state;
    int32_t in_cutscene_mode;
    int32_t minigame_state;
    int32_t shooting_gallery_status;
    int32_t scene;
    int32_t bowling_switch_38;
    int32_t timer_gate_open;
    int32_t countdown_length_gate_open;
    int32_t draw_debug_mode;
    int32_t draw_world_enabled;
    int32_t draw_overlay_enabled;
    int32_t pause_menu_mode;
    int32_t transition_unknown_state;

    // Selected preamble presentation state; these fields do not claim all HUD
    // drawing is pure. Float serialization belongs to the replay observer.
    uint16_t c_up_timer;
    uint16_t c_up_invisible;
    int16_t counter_digits[4];
    int16_t do_action_state;
    uint16_t do_action_current;
    uint16_t do_action_next;
    float do_action_rotation;
    int16_t navi_calling;
    uint16_t a_alpha;
    uint16_t b_alpha;
    uint16_t health_alpha;
    uint16_t magic_alpha;
    uint16_t screen_fill_alpha;
    NativeSimHudPaintObservation paint;
} NativeSimHudObservation;

void Interface_GetNativeSimHudObservation(struct PlayState* play, NativeSimHudObservation* observation);

#ifdef __cplusplus
}
#endif

#endif
