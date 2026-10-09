#pragma once
#include <math.h>
#include <stdint.h>

/* Shared by the native C camera adapter and asset-free contract tests. Hz20
 * never calls these continuations. Gains are per legacy 50-ms opportunity. */
static inline int PlayerCamera_ScaledGain(float legacy, unsigned quanta, float* result) {
    if ((quanta != 1 && quanta != 2) || !isfinite(legacy) || legacy < 0.0f) return 0;
    if (legacy == 0.0f || legacy == 1.0f) *result = legacy;
    else if (legacy < 1.0f) *result = -expm1f(log1pf(-legacy) * ((float)quanta / 6.0f));
    /* Overshooting maps have no real fractional exponential. This explicit
     * local-increment continuation is provisional Class 3, not a semigroup. */
    else *result = legacy * ((float)quanta / 6.0f);
    return isfinite(*result);
}

typedef struct {
    float fraction;
    int16_t previous;
    uint8_t known;
} PlayerCameraAngle;

static inline int PlayerCamera_AdvanceAngle(PlayerCameraAngle* owner, int16_t current,
                                           float increment, int16_t* result) {
    float fraction = owner->known && owner->previous == current ? owner->fraction : 0.0f;
    float total = increment + fraction;
    int32_t whole;
    uint16_t wrapped;
    if (!isfinite(total) || fabsf(total) > 65535.0f) return 0;
    whole = (int32_t)total;
    wrapped = (uint16_t)((uint32_t)(uint16_t)current + (uint32_t)whole);
    *result = (int16_t)(wrapped < 32768 ? (int32_t)wrapped : (int32_t)wrapped - 65536);
    owner->fraction = total - (float)whole;
    owner->previous = *result;
    owner->known = 1;
    return 1;
}

typedef struct {
    int16_t previous;
    uint8_t elapsed;
    uint8_t known;
} PlayerCameraTimer;

static inline float PlayerCamera_Remaining(PlayerCameraTimer* owner, int16_t value) {
    if (!owner->known || owner->previous != value) {
        owner->elapsed = 0;
        owner->previous = value;
        owner->known = 1;
    }
    return (float)value - (float)owner->elapsed / 6.0f;
}

static inline int PlayerCamera_AdvanceTimer(PlayerCameraTimer* owner, int16_t* value, unsigned quanta) {
    if ((quanta != 1 && quanta != 2) || *value < 0) return 0;
    PlayerCamera_Remaining(owner, *value);
    if (*value == 0) { owner->elapsed = 0; return 1; }
    owner->elapsed += (uint8_t)quanta;
    if (owner->elapsed >= 6) { --*value; owner->elapsed -= 6; }
    owner->previous = *value;
    return 1;
}
