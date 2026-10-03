#pragma once
#include "NativeSimulationTest.h"
#include <nlohmann/json.hpp>
const nlohmann::json& NativeSimTest_GetFixture();
void NativeSimTest_TraceJson(const nlohmann::json& record);
void NativeSimTest_ValidateInput();
