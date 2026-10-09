#include "NativeSimulationTest.hpp"
#include "PlayerTemporal.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include "global.h"
#include "player_input.h"
}

namespace {
struct PadEvent {
    uint64_t numerator;
    uint64_t denominator;
    uint32_t sequence;
    uint32_t port;
    OSContPad pad;
};

std::vector<PadEvent> sEvents;
std::array<OSContPad, 4> sPads;
size_t sNextEvent = 0;

int64_t ReadInteger(const nlohmann::json& object, const char* key, int64_t minimum, int64_t maximum,
                    int64_t fallback, bool required = false) {
    auto value = object.find(key);
    if (value == object.end()) {
        if (required) {
            throw std::runtime_error(std::string("input event missing ") + key);
        }
        return fallback;
    }
    if (!value->is_number_integer() ||
        (value->is_number_unsigned() && value->get<uint64_t>() > static_cast<uint64_t>(INT64_MAX))) {
        throw std::runtime_error(std::string("input event ") + key + " must be an integer");
    }
    int64_t result = value->get<int64_t>();
    if (result < minimum || result > maximum) {
        throw std::runtime_error(std::string("input event ") + key + " is outside its supported range");
    }
    return result;
}

bool IsDue(const PadEvent& event, uint64_t timeQ) {
    // Compare rationals without quantizing input onto the 120-unit clock. Splitting
    // whole seconds bounds both products even for long fixture timelines.
    uint64_t eventSeconds = event.numerator / event.denominator;
    uint64_t sampleSeconds = timeQ / 120;
    if (eventSeconds != sampleSeconds) {
        return eventSeconds < sampleSeconds;
    }
    return (event.numerator % event.denominator) * 120 <= (timeQ % 120) * event.denominator;
}

void SamplePad(PadMgr* padMgr, bool worldOpportunity) {
    std::copy(sPads.begin(), sPads.end(), padMgr->pads);
    if (worldOpportunity) PadMgr_ProcessInputs(padMgr);
    else PadMgr_ProcessPlayerInputs(padMgr);
}
} // namespace

void NativeSimTest_ValidateInput() {
    const auto& fixture = NativeSimTest_GetFixture();
    if (fixture.contains("setup_draw_sword") &&
        (!fixture["setup_draw_sword"].is_boolean() ||
         (fixture["setup_draw_sword"].get<bool>() && fixture.value("setup_ticks",60) < 100)))
        throw std::runtime_error("setup_draw_sword requires a boolean and at least 100 setup ticks");
    sEvents.clear();
    sNextEvent = 0;
    sPads = {};
    for (size_t port = 1; port < sPads.size(); ++port) {
        sPads[port].err_no = 8;
    }
    const auto input = fixture.find("input");
    if (input == fixture.end() || !input->is_array()) {
        throw std::runtime_error("fixture input must be an array of full pad states");
    }
    for (const auto& entry : *input) {
        if (!entry.is_object()) {
            throw std::runtime_error("input event must be an object");
        }
        PadEvent event{};
        // These bounds keep all timestamp comparison products inside uint64_t.
        event.numerator = ReadInteger(entry, "time_num", 0, 1000000000, 0, true);
        event.denominator = ReadInteger(entry, "time_den", 1, 1000000000, 1, true);
        event.sequence = static_cast<uint32_t>(ReadInteger(entry, "sequence", 0, UINT32_MAX, 0, true));
        event.port = static_cast<uint32_t>(ReadInteger(entry, "port", 0, 3, 0));
        event.pad.button = static_cast<uint32_t>(ReadInteger(entry, "buttons", 0, UINT32_MAX, 0, true));
        event.pad.stick_x = static_cast<int8_t>(ReadInteger(entry, "stick_x", -128, 127, 0, true));
        event.pad.stick_y = static_cast<int8_t>(ReadInteger(entry, "stick_y", -128, 127, 0, true));
        event.pad.right_stick_x = static_cast<int8_t>(ReadInteger(entry, "right_stick_x", -128, 127, 0));
        event.pad.right_stick_y = static_cast<int8_t>(ReadInteger(entry, "right_stick_y", -128, 127, 0));
        if (entry.contains("connected") && !entry["connected"].is_boolean()) {
            throw std::runtime_error("input connected must be a boolean");
        }
        event.pad.err_no = entry.value("connected", true) ? 0 : 8;
        if (event.pad.err_no && (event.pad.button != 0 || event.pad.stick_x != 0 || event.pad.stick_y != 0 ||
                                event.pad.right_stick_x != 0 || event.pad.right_stick_y != 0)) {
            throw std::runtime_error("disconnected input must supply neutral buttons and axes");
        }
        // Gyro remains zero in schema 1; fail explicitly instead of silently
        // accepting a replay field that this normalized-pad provider ignores.
        if (entry.contains("gyro_x") || entry.contains("gyro_y") || entry.contains("gyro_x_bits") ||
            entry.contains("gyro_y_bits")) {
            throw std::runtime_error("gyro input is not supported by fixture schema 1");
        }
        if (!sEvents.empty()) {
            const PadEvent& previous = sEvents.back();
            if (event.numerator * previous.denominator < previous.numerator * event.denominator ||
                event.sequence <= previous.sequence) {
                throw std::runtime_error("input events require nondecreasing timestamps and strictly increasing sequence");
            }
        }
        sEvents.push_back(event);
    }
}

static int ReplayPadAt(PadMgr* padMgr, uint64_t timeQ, const char* site, bool worldOpportunity) {
    if (!NativeSimTest_IsEnabled()) {
        return 0;
    }
    NativeSimTest_Event("input_poll", site, 0);
    if (worldOpportunity && padMgr->retraceCallback) {
        padMgr->retraceCallback(padMgr, padMgr->retraceCallbackValue);
    }
    bool sampled = false;
    if (NativeSimTest_IsMeasuring()) {
        while (sNextEvent < sEvents.size() && IsDue(sEvents[sNextEvent], timeQ)) {
            const PadEvent& event = sEvents[sNextEvent++];
            const uint32_t previous = sPads[event.port].button;
            PlayerTemporal_InputSample(event.sequence, event.numerator, event.denominator, event.port,
                event.pad.button, static_cast<uint16_t>((previous ^ event.pad.button) & event.pad.button),
                static_cast<uint16_t>((previous ^ event.pad.button) & previous));
            sPads[event.port] = event.pad;
            // Each transition passes through the real edge accumulator. A press
            // and release between steps therefore retains both masks, with the
            // final held state up. No extra game update is synthesized.
            SamplePad(padMgr, worldOpportunity);
            sampled = true;
            NativeSimTest_TraceJson({ { "kind", "input_event" },
                                      { "site", site },
                                      { "input_sequence", event.sequence },
                                      { "input_time_num", event.numerator },
                                      { "input_time_den", event.denominator },
                                      { "applied_time_q", timeQ },
                                      { "port", event.port },
                                      { "buttons", event.pad.button },
                                      { "stick_x", event.pad.stick_x },
                                      { "stick_y", event.pad.stick_y },
                                      { "right_stick_x", event.pad.right_stick_x },
                                      { "right_stick_y", event.pad.right_stick_y },
                                      { "connected", event.pad.err_no == 0 } });
        }
    }
    if (!NativeSimTest_IsMeasuring() && NativeSimTest_GetFixture().value("setup_draw_sword",false)) {
        // Prepare an unsheathed idle sword through the ordinary pad/action path.
        // No direct action, animation, equipment or weapon-history assignment.
        const auto frame = NativeSimTest_SetupFrame();
        sPads[0].button = frame >= 30 && frame < 35 ? BTN_B : 0;
    }
    if (!sampled) {
        SamplePad(padMgr, worldOpportunity);
    }
    padMgr->validCtrlrsMask = 0;
    for (size_t port = 0; port < sPads.size(); ++port) {
        padMgr->padStatus[port].type = CONT_TYPE_NORMAL;
        padMgr->padStatus[port].status = 0;
        padMgr->padStatus[port].err_no = sPads[port].err_no;
        if (sPads[port].err_no == 0) {
            padMgr->validCtrlrsMask |= 1U << port;
        }
    }
    // Physical rumble and host controller queries are intentionally absent from
    // the deterministic provider; the retrace callback belongs only to world20.
    return 1;
}

extern "C" int NativeSimTest_ReplayPad(PadMgr* padMgr) {
    return ReplayPadAt(padMgr, NativeSimTest_TimeQ(), "PadMgr_HandleRetraceMsg", true);
}

extern "C" int NativeSimTest_ReplayPlayerPad(PadMgr* padMgr, uint64_t timeQ) {
    return ReplayPadAt(padMgr, timeQ, "PadMgr_PollPlayer", false);
}
