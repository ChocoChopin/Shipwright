#pragma once
#include "PlayerTemporalCore.hpp"
#include <cmath>

namespace PlayerTemporal {
// Only the admitted Player adapter uses this continuation. Velocity retains its
// legacy units; its displacement gain is 1.5 per 50 ms. Gravity is an increment
// of that stored velocity per 50 ms. Root motion never enters this operation.
struct PlayerMotion {
    std::array<float, 3> position{}, velocity{};
    float gravity = 0, terminalVelocity = -20;
};
inline bool AdvancePlayerAngle(int16_t& angle, int16_t target, int32_t canonicalCap,
                              unsigned quanta, RateRemainder& residue, bool& reached) {
    if ((quanta != 1 && quanta != 2) || canonicalCap < 0 || canonicalCap > INT16_MAX) return false;
    const uint16_t wrapped = uint16_t(uint16_t(angle) - uint16_t(target));
    const int32_t difference = wrapped < 32768 ? wrapped : int32_t(wrapped) - 65536;
    if (!difference) { reached = true; residue.Reset(); return true; }
    int64_t increment;
    if (!residue.Advance(difference > 0 ? -canonicalCap : canonicalCap, quanta, increment)) return false;
    reached = std::abs(increment) >= std::abs(difference);
    if (reached) { angle = target; residue.Reset(); }
    else angle = static_cast<int16_t>(WrapAngle(uint16_t(angle), increment));
    return true;
}
inline bool AdvancePlayerMotion(PlayerMotion& state, const PlayerStepContext& step,
                                const std::array<float, 3>& worldCorrection) {
    if ((step.rate != SimulationRate::Hz60 && step.rate != SimulationRate::Hz120) ||
        step.stepQuanta != StepQuanta(step.rate) || step.endTime.quanta < step.startTime.quanta ||
        step.endTime.quanta - step.startTime.quanta != step.stepQuanta) return false;
    auto next = state;
    const float duration = float(step.stepQuanta) / 6.0f;
    const float displacementGain = float(step.stepQuanta) * 0.25f;
    if (!std::isfinite(state.gravity) || !std::isfinite(state.terminalVelocity)) return false;
    const bool terminalHold = state.velocity[1] == state.terminalVelocity && state.gravity <= 0;
    next.velocity[1] = terminalHold ? state.terminalVelocity : state.velocity[1] + state.gravity * duration;
    // A clamp-crossing interval is outside this constant-force segment. The
    // whole-profile gate must reject it before Player work, not fall back here.
    if (state.velocity[1] < state.terminalVelocity || next.velocity[1] < state.terminalVelocity) return false;
    for (unsigned axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(state.position[axis]) || !std::isfinite(state.velocity[axis]) ||
            !std::isfinite(worldCorrection[axis])) return false;
        // World OC correction is already a displacement, applied once at the
        // shared boundary. Intermediate static queries supply their own snaps.
        const float correction = CommonBoundary(step.startTime) ? worldCorrection[axis] : 0.0f;
        float displacement = next.velocity[axis] * displacementGain;
        if (axis == 1 && !terminalHold) {
            // Fractional continuation of v'=v+g, y'=y+1.5*v'. Its constant-force
            // segments compose and preserve the canonical 50-ms endpoints.
            displacement = 1.5f * (duration * state.velocity[1] +
                                   state.gravity * duration * (duration + 1.0f) * 0.5f);
        }
        next.position[axis] += displacement + correction;
        if (!std::isfinite(next.position[axis]) || !std::isfinite(next.velocity[axis])) return false;
    }
    state = next;
    return true;
}
}
