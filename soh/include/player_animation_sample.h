#ifndef PLAYER_ANIMATION_SAMPLE_H
#define PLAYER_ANIMATION_SAMPLE_H
#include <stdint.h>

/* Rz(z) Ry(y) Rx(x) also represents (x + pi, pi - y, z + pi).
 * Animation keys may change Euler branch without changing the physical pose.
 * Choose the nearer branch before fractional sampling, not a half-turn detour.
 * Vec3s must be declared by the caller. No state or floating-point timing. */
static inline Vec3s PlayerAnimation_NearestEuler(Vec3s start, Vec3s target) {
    Vec3s alternate = { (int16_t)(target.x + 0x8000), (int16_t)(0x8000 - target.y),
                       (int16_t)(target.z + 0x8000) };
    int64_t dx = (int16_t)(target.x - start.x), dy = (int16_t)(target.y - start.y);
    int64_t dz = (int16_t)(target.z - start.z);
    int64_t ax = (int16_t)(alternate.x - start.x), ay = (int16_t)(alternate.y - start.y);
    int64_t az = (int16_t)(alternate.z - start.z);
    return ax * ax + ay * ay + az * az < dx * dx + dy * dy + dz * dz ? alternate : target;
}
#endif
