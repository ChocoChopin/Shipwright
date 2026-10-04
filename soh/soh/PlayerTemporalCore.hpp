#pragma once
// Pass 4A foundations: these helpers never execute gameplay or choose its cadence.
#include <array>
#include <cstdint>
#include <deque>
#include <limits>

namespace PlayerTemporal {
constexpr uint64_t QuantaPerSecond = 120;
constexpr uint32_t WorldStepQuanta = 6;
enum class SimulationRate : uint16_t { Hz20 = 20, Hz60 = 60, Hz120 = 120 };
constexpr bool ValidRate(unsigned hz) { return hz == 20 || hz == 60 || hz == 120; }
constexpr uint32_t StepQuanta(SimulationRate rate) {
    return rate == SimulationRate::Hz20 ? 6 : rate == SimulationRate::Hz60 ? 2 :
           rate == SimulationRate::Hz120 ? 1 : 0;
}
constexpr bool LegacyArithmetic(SimulationRate rate) { return rate == SimulationRate::Hz20; }
struct SimTime { uint64_t quanta = 0; };
struct SimDuration { uint64_t quanta = 0; }; // No sentinel: expiration is separate from disabled.
inline bool Add(SimTime start, SimDuration duration, SimTime& end) {
    if (duration.quanta > UINT64_MAX - start.quanta) return false;
    end.quanta = start.quanta + duration.quanta;
    return true;
}
constexpr bool CommonBoundary(SimTime t) { return t.quanta % WorldStepQuanta == 0; }
struct Identity { uint64_t scene = 0, player = 0, scope = 0; };
constexpr bool operator==(Identity a, Identity b) {
    return a.scene == b.scene && a.player == b.player && a.scope == b.scope;
}
struct PlayerStepContext {
    SimulationRate rate = SimulationRate::Hz20;
    uint32_t stepQuanta = WorldStepQuanta;
    SimTime startTime{}, endTime{};
    uint64_t playerStepId = 0;
    Identity identity{};
};
struct WorldStepContext {
    uint32_t stepQuanta = WorldStepQuanta;
    SimTime startTime{}, endTime{};
    uint64_t worldStepId = 0, sceneEpoch = 0;
};
struct Capability {
    SimulationRate requestedPlayerRate = SimulationRate::Hz20;
    // Deliberately not writable fields: no production high-rate admission in 4A.
    constexpr SimulationRate EffectiveRate() const { return SimulationRate::Hz20; }
    constexpr unsigned WorldRate() const { return 20; }
    constexpr bool HighRateAdmitted() const { return false; }
    bool Request(unsigned hz) {
        if (!ValidRate(hz)) return false;
        requestedPlayerRate = static_cast<SimulationRate>(hz); return true;
    }
};
enum class Domain : uint8_t { Player, Animation, World, Hud, AudioLogical, Presentation };
struct LegacyOpportunity {
    Domain domain = Domain::World;
    Identity identity{};
    uint64_t index = 0;
    SimTime due{};
};
// One cursor per authored source/marker. A true condition is not a new index.
class OpportunityCursor {
    Identity owner{};
    Domain domain = Domain::World;
    uint64_t last = 0;
    bool fired = false;
  public:
    void Reset(Identity next, Domain source) { owner = next; domain = source; last = 0; fired = false; }
    bool Consume(const LegacyOpportunity& op, SimTime now) {
        if (!(op.identity == owner) || op.domain != domain || op.due.quanta > now.quanta ||
            (fired && op.index <= last)) return false;
        last = op.index; fired = true; return true;
    }
};
// Guard each named world source with its own cursor. No implicit catch-up or dropped debt.
inline bool ConsumeWorld(OpportunityCursor& cursor, const WorldStepContext& world,
                         Identity owner, SimTime now, Domain source = Domain::World) {
    if (world.sceneEpoch != owner.scene || world.stepQuanta != WorldStepQuanta ||
        !CommonBoundary(world.startTime) || world.endTime.quanta < world.startTime.quanta ||
        world.endTime.quanta - world.startTime.quanta != WorldStepQuanta) return false;
    return cursor.Consume({source, owner, world.worldStepId, world.startTime}, now);
}
// Duration in source-clock quanta; reset by its owning action. No gameplay field is converted.
struct Countdown {
    SimDuration remaining{};
    bool enabled = false;
    void Reset(SimDuration value) { remaining = value; enabled = true; }
    bool Advance(SimDuration elapsed) {
        if (!enabled) return false;
        if (elapsed.quanta < remaining.quanta) { remaining.quanta -= elapsed.quanta; return false; }
        remaining.quanta = 0; enabled = false; return true;
    }
};
// Signed legacy integer increment -> integral output + retained sixths. Scaling
// belongs solely to this accumulator. Reset on owner/rate change; Hz20 callers
// retain their original expression. numerator=4 is math-only s=2/3, not a rate.
struct RateRemainder {
    int64_t sixths = 0;
    bool Advance(int32_t legacyIncrement, uint32_t numerator, int64_t& whole) {
        if (numerator < 1 || numerator > 6 || sixths <= -6 || sixths >= 6) return false;
        int64_t total = int64_t(legacyIncrement) * numerator + sixths;
        whole = total / 6; sixths = total % 6; return true; // C++ truncation toward zero
    }
    void Reset() { sixths = 0; }
};
inline uint16_t WrapAngle(uint16_t angle, int64_t delta) {
    return static_cast<uint16_t>(uint64_t(angle) + static_cast<uint64_t>(delta));
}
// Signed, unwrapped Q16 authored frames, not elapsed seconds. Intervals exclude
// their start and include their end in either direction. Caller supplies wraps;
// wrapped frame samples alone cannot identify how many loops occurred.
constexpr int64_t PhaseLimit = int64_t(1) << 50;
inline int64_t FloorDiv(int64_t value, int64_t divisor) {
    int64_t q = value / divisor; return q - (value % divisor < 0 ? 1 : 0);
}
struct MarkerRange { int64_t firstLoop = 0; uint64_t count = 0; int direction = 0; };
inline bool Crossings(int64_t from, int64_t to, int64_t marker, int64_t period, MarkerRange& result) {
    if (from < -PhaseLimit || from > PhaseLimit || to < -PhaseLimit || to > PhaseLimit ||
        period <= 0 || period > PhaseLimit || marker < 0 || marker >= period) return false;
    result = {};
    if (to > from) {
        auto first = FloorDiv(from - marker, period) + 1;
        auto last = FloorDiv(to - marker, period);
        result = {first, uint64_t(last - first + 1), 1};
    } else if (to < from) {
        auto first = -FloorDiv(-(from - marker), period) - 1;
        auto last = -FloorDiv(-(to - marker), period);
        result = {first, uint64_t(first - last + 1), -1};
    }
    return true;
}
// Interval identity is allocated once by the animation owner, never per query.
// Per-marker cursors permit several different authored markers in one interval.
struct AnimationEvents {
    uint64_t generation = 0, interval = 0;
    std::array<uint64_t, 16> consumed{};
    bool Change() {
        if (generation == UINT64_MAX) return false;
        ++generation; interval = 0; consumed = {}; return true;
    }
    bool Advance() { if (interval == UINT64_MAX) return false; ++interval; return true; }
    bool Consume(unsigned markerId, uint64_t expectedGeneration, uint64_t expectedInterval,
                 const MarkerRange& crossings) {
        if (markerId >= consumed.size() || !crossings.count || expectedGeneration != generation ||
            !interval || expectedInterval != interval || consumed[markerId] == interval) return false;
        consumed[markerId] = interval; return true;
    }
};
struct AttackState {
    uint64_t epoch = 0, hitOpportunity = 0;
    bool attacking = false, activeWindow = false;
    bool Begin() {
        if (epoch == UINT64_MAX) return false;
        ++epoch; hitOpportunity = 0; attacking = true; activeWindow = false; return true;
    }
    bool Window(bool active) {
        if (active && !activeWindow && attacking) {
            if (hitOpportunity == UINT64_MAX) return false;
            ++hitOpportunity;
        }
        activeWindow = active && attacking; return true;
    }
    void End() { attacking = activeWindow = false; } // retain identity for observations; never reset sword history
};
struct Lifecycle {
    Identity identity{};
    AttackState attack{};
    AnimationEvents animation{};
    bool playerAlive = false;
    bool Invalidate() {
        if (identity.scope == UINT64_MAX) return false;
        ++identity.scope; attack.End(); return animation.Change();
    }
    bool Scene() {
        if (identity.scene == UINT64_MAX) return false;
        ++identity.scene; playerAlive = false; return Invalidate();
    }
    bool CreatePlayer() {
        if (identity.player == UINT64_MAX) return false;
        ++identity.player; playerAlive = true; return Invalidate();
    }
    bool DestroyPlayer() { playerAlive = false; return Invalidate(); }
};
// Acquisition timestamp is rational seconds relative to a declared source epoch.
// Input availability and consumption are separate; edges retain canonical 16-bit
// PadMgr masks, held state keeps 32 bits. Queue never overwrites unseen events.
struct InputEvent {
    Identity identity{};
    uint64_t sequence = 0, timeNumerator = 0, timeDenominator = 1;
    SimTime available{};
    uint32_t held = 0;
    uint16_t pressed = 0, released = 0;
    uint8_t port = 0;
};
class InputTimeline {
    Identity owner{};
    std::deque<InputEvent> queue;
    uint64_t lastQueued = 0;
    bool hasQueued = false;
  public:
    uint64_t consumedSequence = 0, consumedEdges = 0, consumingPlayerStep = 0;
    size_t Queued() const { return queue.size(); }
    void Reset(Identity next) {
        owner = next; queue.clear(); hasQueued = false; lastQueued = 0;
        consumedSequence = consumedEdges = consumingPlayerStep = 0;
    }
    bool Queue(InputEvent event) {
        if (!(event.identity == owner) || event.port >= 4 || !event.timeDenominator ||
            queue.size() >= 256 || (hasQueued && event.sequence <= lastQueued) ||
            (!queue.empty() && event.available.quanta < queue.back().available.quanta)) return false;
        lastQueued = event.sequence; hasQueued = true; queue.push_back(event); return true;
    }
    bool Consume(SimTime start, InputEvent& event) {
        if (queue.empty() || queue.front().available.quanta > start.quanta) return false;
        event = queue.front(); queue.pop_front(); consumedSequence = event.sequence;
        if (event.pressed || event.released) ++consumedEdges;
        return true;
    }
    // Future Player delivery contract. Multiple distinct edges may share one
    // step; an edge removed here cannot be delivered again on another substep.
    bool ConsumeForPlayer(const PlayerStepContext& step, InputEvent& event) {
        if (!(step.identity == owner) || !step.playerStepId || step.playerStepId < consumingPlayerStep ||
            !StepQuanta(step.rate) || step.stepQuanta != StepQuanta(step.rate) ||
            step.endTime.quanta < step.startTime.quanta ||
            step.endTime.quanta - step.startTime.quanta != step.stepQuanta) return false;
        if (!Consume(step.startTime, event)) return false;
        consumingPlayerStep = step.playerStepId; return true;
    }
};
enum class ResponseState : uint8_t { Detected, Reserved, Committed, Rejected };
// Data contract only; no queue/geometry producer/damage consumer exists in 4A.
struct ContactEvent {
    Identity owner{};
    uint64_t attackEpoch = 0, hitOpportunity = 0, playerStepId = 0;
    SimTime time{};
    uint64_t sequence = 0, worldProxyGeneration = 0, targetGeneration = 0;
    uint32_t attackerCollider = 0, attackerElement = 0, targetCollider = 0, targetElementGroup = 0;
    uint32_t damageFlags = 0, attackAnimation = 0;
    float hitPosition[3]{}, normal[3]{};
    uint8_t material = 0, effect = 0;
    bool bounced = false;
    ResponseState response = ResponseState::Detected;
};
inline bool SameHitOpportunity(const ContactEvent& a, const ContactEvent& b) {
    return a.owner == b.owner && a.attackEpoch == b.attackEpoch && a.hitOpportunity == b.hitOpportunity &&
           a.targetGeneration == b.targetGeneration && a.targetElementGroup == b.targetElementGroup;
}
// QA controls admit an entire canonical transaction before ANY input/update/draw.
// Pause creates no debt and freezes input acquisition. It never touches frameAdvCtx.
struct CanonicalControl {
    bool paused = false, stepPending = false, inFlight = false;
    SimTime time{};
    uint64_t transactionId = 0;
    void Pause() { paused = true; stepPending = false; }
    void Run() { paused = false; stepPending = false; }
    bool Step() { if (!paused || inFlight || stepPending) return false; stepPending = true; return true; }
    bool Begin() {
        if (inFlight || (paused && !stepPending) || transactionId == UINT64_MAX ||
            time.quanta > UINT64_MAX - WorldStepQuanta)
            return false;
        inFlight = true; stepPending = false; return true;
    }
    bool Commit() {
        if (!inFlight) return false;
        time.quanta += WorldStepQuanta; ++transactionId; inFlight = false; return true;
    }
};
} // namespace PlayerTemporal
