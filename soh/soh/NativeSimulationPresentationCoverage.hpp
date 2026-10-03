#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

// Diagnostic bookkeeping only. A missing helper key initially yields JSON null;
// value() requires an object even when a fallback value is supplied.
inline void NativeSimRecordPresentationCoverage(nlohmann::json& helpers, const char* helper,
                                                bool measuring, bool visible, uint64_t commands) {
    auto& coverage = helpers[helper];
    if (coverage.is_null()) coverage = nlohmann::json::object();
    const char* period = measuring ? "measured" : "setup";
    coverage[period] = coverage.value(period, uint64_t{0}) + 1;
    const char* visibility = visible ? "visible" : "invisible";
    coverage[visibility] = coverage.value(visibility, uint64_t{0}) + 1;
    coverage["commands"] = coverage.value("commands", uint64_t{0}) + commands;
}
