#include "NativeSimulationPresentationCoverage.hpp"
#include <cstdio>
#include <exception>

int main() {
    int checks = 0;
    auto require = [&](bool condition) {
        ++checks;
        if (!condition) throw std::runtime_error("coverage assertion failed");
    };
    try {
        auto helpers = nlohmann::json::object();
        NativeSimRecordPresentationCoverage(helpers, "countdown", false, false, 0);
        require(helpers.at("countdown").is_object());
        require(helpers.at("countdown").at("setup") == 1);
        require(helpers.at("countdown").at("invisible") == 1);
        require(helpers.at("countdown").at("commands") == 0);
        require(!helpers.at("countdown").contains("measured"));
        require(!helpers.at("countdown").contains("visible"));
        NativeSimRecordPresentationCoverage(helpers, "countdown", true, true, 60);
        NativeSimRecordPresentationCoverage(helpers, "countdown", true, true, 70);
        require(helpers.at("countdown").at("setup") == 1);
        require(helpers.at("countdown").at("measured") == 2);
        require(helpers.at("countdown").at("visible") == 2);
        require(helpers.at("countdown").at("invisible") == 1);
        require(helpers.at("countdown").at("commands") == 130);
        NativeSimRecordPresentationCoverage(helpers, "message", true, true, 900);
        require(helpers.at("message").is_object());
        require(helpers.at("message").at("measured") == 1);
        require(helpers.at("message").at("visible") == 1);
        require(helpers.at("message").at("commands") == 900);
        require(helpers.at("countdown").at("commands") == 130);
        NativeSimRecordPresentationCoverage(helpers, "message", false, false, 3);
        require(helpers.at("message").at("setup") == 1);
        require(helpers.at("message").at("invisible") == 1);
        require(helpers.at("message").at("commands") == 903);
        require(nlohmann::json::parse(helpers.dump()) == helpers);
        std::printf("Purity coverage: PASS; %d checks\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Purity coverage: FAIL; check %d: %s\n", checks, error.what());
        return 1;
    }
}
