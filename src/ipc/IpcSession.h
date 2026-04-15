#pragma once

#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>
#include "../core/ConfigManager.h"

namespace MoLab {

class SimulationEngine;
class Logger;

enum class LogLevel;

struct IpcThrottleSettings {
    uint64_t tick_event_interval = 50;
    uint64_t telemetry_interval_ticks = 5;
};

std::string logLevelToString(LogLevel level);

void applyLogLevel(Logger& logger, const std::string& log_level);

bool resolveIpcThrottleSettings(const SimulationConfig& simulation_config,
                                const nlohmann::json& payload,
                                IpcThrottleSettings& settings,
                                std::string* error = nullptr);

int runIpcStdioSession(SimulationEngine& engine, Logger& logger);

} // namespace MoLab
