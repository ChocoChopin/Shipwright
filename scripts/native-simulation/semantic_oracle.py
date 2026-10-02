"""Pure mathematical reference models; NOT a Shipwright gameplay harness.

Time is in integer/fractional 1/120-second quanta. Fractions intentionally avoid
floating-point drift in the reference algebra. Production 20-Hz code must retain
its actual operation ordering and rounding; this oracle cannot prove that.
"""

from fractions import Fraction
from math import ceil, expm1, isfinite, lcm, log1p

SUPPORTED_RATES = (20, 30, 60, 120)
QUANTA_PER_SECOND = 120
LEGACY_FRAME_QUANTA = 6


def step_quanta(rate: int) -> int:
    if not isinstance(rate, int) or rate not in SUPPORTED_RATES:
        raise ValueError("rate must be 20, 30, 60, or 120")
    return QUANTA_PER_SECOND // rate


def step_scale(rate: int) -> Fraction:
    return Fraction(step_quanta(rate), LEGACY_FRAME_QUANTA)


def common_boundary_quanta(rates=SUPPORTED_RATES) -> int:
    """All four rates share authoritative boundaries every 0.1 seconds."""
    return lcm(*(step_quanta(rate) for rate in rates))


def next_boundary(due_quanta: Fraction, rate: int) -> int:
    """Causal dispatch: exact boundaries dispatch there, never before due time."""
    if due_quanta < 0:
        raise ValueError("negative timestamps are outside this reference model")
    quantum = step_quanta(rate)
    return ceil(Fraction(due_quanta) / quantum) * quantum


def periodic_schedule(period_frames: int, end_quanta: int, rate: int):
    """Return (exact_due, dispatch_boundary) using absolute deadlines.

    First event is due after one period; never reschedule from a late dispatch.
    Actual ordering relative to actor logic is a separate per-callsite contract.
    """
    if not isinstance(period_frames, int) or period_frames <= 0:
        raise ValueError("period_frames must be a positive integer")
    if end_quanta < 0:
        raise ValueError("end_quanta must be nonnegative")
    step_quanta(rate)
    period = period_frames * LEGACY_FRAME_QUANTA
    return [(due, next_boundary(Fraction(due), rate))
            for due in range(period, end_quanta + 1, period)]


def linear_increment(legacy_increment, rate: int) -> Fraction:
    return Fraction(legacy_increment) * step_scale(rate)


def affine_acceleration(position, velocity, acceleration, rate: int,
                        position_coefficient=1):
    """Fractional iterate of v'=v+a; x'=x+c*v' for CONSTANT a and c.

    This selected reference option preserves the old discrete-map endpoints.
    It is not a global gravity conversion and excludes clamps, collisions,
    changing forces, state transitions, and 16-bit/integer quantization.
    """
    x, v, a, c = map(Fraction, (position, velocity, acceleration,
                               position_coefficient))
    scale = step_scale(rate)
    if rate == 20:
        new_velocity = v + a
        return x + c * new_velocity, new_velocity
    return x + c * (scale * v + a * scale * (scale + 1) / 2), v + scale * a


def decay_factor(legacy_factor: float, rate: int) -> float:
    """Positive real exponential family; negative factors need another model."""
    scale = step_scale(rate)
    if not isfinite(legacy_factor) or legacy_factor < 0:
        raise ValueError("factor must be finite and nonnegative")
    if rate == 20:
        return legacy_factor
    return legacy_factor ** float(scale)


def smoothing_alpha(legacy_alpha: float, rate: int) -> float:
    """Stationary-target linear smoothing only; excludes clamps/minimum steps."""
    if not isfinite(legacy_alpha) or not 0 <= legacy_alpha <= 1:
        raise ValueError("alpha must lie in [0, 1]")
    step_quanta(rate)
    if rate == 20 or legacy_alpha in (0.0, 1.0):
        return legacy_alpha
    return -expm1(float(step_scale(rate)) * log1p(-legacy_alpha))


def opportunity_probability(legacy_probability: float, rate: int) -> float:
    """Equal no-event survival probability; NOT equivalent RNG draw ordering."""
    return smoothing_alpha(legacy_probability, rate)
