#pragma once
#include "PlayerTemporalCore.hpp"

namespace PlayerTemporal {
// Bounded history includes completed reservations until the attack changes.
// Quad and Player-step identity intentionally do not participate in dedup.
struct ContactReservations {
    std::array<ContactEvent, 32> events{};
    size_t count = 0;
    uint64_t sequence = 0, detected = 0, duplicates = 0, reserved = 0, committed = 0, rejected = 0;
    bool Reserve(ContactEvent candidate) {
        ++detected;
        for (size_t i = 0; i < count; ++i) {
            if (SameHitOpportunity(events[i], candidate)) { ++duplicates; return false; }
        }
        size_t keep = 0;
        for (size_t i = 0; i < count; ++i)
            if (events[i].response == ResponseState::Reserved ||
                (events[i].owner == candidate.owner && events[i].attackEpoch == candidate.attackEpoch))
                events[keep++] = events[i];
        count = keep;
        if (count == events.size() || sequence == UINT64_MAX) return false;
        candidate.sequence = ++sequence;
        candidate.response = ResponseState::Reserved;
        events[count++] = candidate; ++reserved; return true;
    }
    void Resolve(ContactEvent& event, bool valid) {
        if (event.response != ResponseState::Reserved) return;
        event.response = valid ? ResponseState::Committed : ResponseState::Rejected;
        if (valid) ++committed; else ++rejected;
    }
    void Invalidate() {
        for (size_t i = 0; i < count; ++i) Resolve(events[i], false);
    }
    size_t Pending() const {
        size_t pending = 0;
        for (size_t i = 0; i < count; ++i) pending += events[i].response == ResponseState::Reserved;
        return pending;
    }
};
}
