#pragma once
#include "PlayerTemporalCore.hpp"

namespace PlayerTemporal {
// An explicitly owned legacy-frequency opportunity inside a finer Player clock.
// A reset can anchor it to a newly authored event, rather than world tick parity.
// Lifecycle/scope invalidation resets the owner; gameplay counters remain live.
class PeriodicPlayerOpportunity {
    bool initialized = false;
    SimTime due{};
    uint64_t consumedStep = 0;
  public:
    bool Reset(SimTime start, bool immediate) {
        SimTime next;
        if (!Add(start, {immediate ? 0u : WorldStepQuanta}, next)) return false;
        initialized = true; due = next; consumedStep = 0; return true;
    }
    bool Consume(SimTime now, uint64_t stepId) {
        if (!stepId || stepId <= consumedStep || (initialized && now.quanta < due.quanta)) return false;
        SimTime next;
        if (!Add(now, {WorldStepQuanta}, next)) return false;
        initialized = true; due = next; consumedStep = stepId; return true;
    }
};

// Owns interval identity only. Engine adapters must establish complete profile
// admission before BeginWorld and perform the declared work before each commit.
// A boundary Player step is split around world suffix/animation/camera/late pose;
// this clock never dispatches an intact Player update after a whole world tick.
class FixedPlayerClock {
    Identity owner{};
    SimulationRate requested = SimulationRate::Hz20, effective = SimulationRate::Hz20;
    SimTime now{};
    WorldStepContext world{};
    PlayerStepContext player{};
    uint64_t playerId = 0, worldId = 0;
    bool worldOpen = false, playerOpen = false, revokePending = false, fallbackLatched = false;
  public:
    bool Reset(Identity identity, SimTime start = {}, uint64_t priorPlayerId = 0, uint64_t priorWorldId = 0) {
        if (worldOpen || playerOpen || !CommonBoundary(start)) return false;
        owner = identity; now = start; world = {}; player = {};
        playerId = priorPlayerId; worldId = priorWorldId; requested = effective = SimulationRate::Hz20;
        revokePending = fallbackLatched = false;
        return true;
    }
    bool Request(unsigned hz) {
        if (worldOpen || !ValidRate(hz)) return false;
        requested = static_cast<SimulationRate>(hz);
        return true;
    }
    bool BeginWorld(bool wholeProfileAdmitted) {
        SimTime end;
        if (worldOpen || playerOpen || !CommonBoundary(now) || worldId == UINT64_MAX ||
            !Add(now, {WorldStepQuanta}, end)) return false;
        effective = wholeProfileAdmitted && !fallbackLatched ? requested : SimulationRate::Hz20;
        world = {WorldStepQuanta, now, end, worldId + 1, owner.scene};
        worldOpen = true; revokePending = false;
        return true;
    }
    bool BeginPlayer() {
        if (!worldOpen || playerOpen || revokePending || now.quanta >= world.endTime.quanta ||
            playerId == UINT64_MAX) return false;
        SimTime end;
        const auto step = StepQuanta(effective);
        if (!Add(now, {step}, end) || end.quanta > world.endTime.quanta) return false;
        player = {effective, step, now, end, playerId + 1, owner};
        playerOpen = true;
        return true;
    }
    bool CommitPlayer() {
        if (!playerOpen) return false;
        now = player.endTime; playerId = player.playerStepId; playerOpen = false;
        return true;
    }
    // Called after a complete committed step or before the next step. Never
    // converts half an interval. Remaining Player starts are withheld; the next
    // shared world boundary resumes exact canonical scheduling. No event queue
    // is cleared here. Re-entry requires an explicit scene/session Reset.
    bool RevokeAdmission() {
        if (!worldOpen || playerOpen || effective == SimulationRate::Hz20) return false;
        revokePending = fallbackLatched = true;
        return true;
    }
    bool EndWorld() {
        if (!worldOpen || playerOpen || (!revokePending && now.quanta != world.endTime.quanta)) return false;
        now = world.endTime; worldId = world.worldStepId; worldOpen = false;
        if (revokePending) effective = SimulationRate::Hz20;
        return true;
    }
    bool BoundaryPlayer() const { return playerOpen && player.startTime.quanta == world.startTime.quanta; }
    bool WorldOpen() const { return worldOpen; }
    bool PlayerOpen() const { return playerOpen; }
    bool FallbackLatched() const { return fallbackLatched; }
    SimTime Now() const { return now; }
    SimulationRate Requested() const { return requested; }
    SimulationRate Effective() const { return effective; }
    const WorldStepContext& World() const { return world; }
    const PlayerStepContext& Player() const { return player; }
};

// A grant is consumed by a complete Player interval, never by presentation.
// NextWorld includes all due Player intervals and stops before the next world
// transaction. Caller still executes the one due world opportunity in its slot.
class PlayerStepControl {
    enum class Mode { Paused, OnePlayer, NextWorld, Running } mode = Mode::Running;
    uint64_t stopWorld = 0;
    bool open = false;
  public:
    bool Pause() { if (open) return false; mode = Mode::Paused; return true; }
    bool Run() { if (open) return false; mode = Mode::Running; return true; }
    bool Step() { if (open || mode != Mode::Paused) return false; mode = Mode::OnePlayer; return true; }
    bool NextWorld(uint64_t dueWorld) {
        if (open || mode != Mode::Paused || dueWorld == 0) return false;
        stopWorld = dueWorld; mode = Mode::NextWorld; return true;
    }
    bool Begin(uint64_t dueWorld) {
        if (open || mode == Mode::Paused || (mode == Mode::NextWorld && dueWorld > stopWorld)) return false;
        open = true; return true;
    }
    bool Commit(bool worldEndpoint) {
        if (!open) return false;
        open = false;
        if (mode == Mode::OnePlayer || (mode == Mode::NextWorld && worldEndpoint)) mode = Mode::Paused;
        return true;
    }
};
} // namespace PlayerTemporal
