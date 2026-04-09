#include "IpcSession.h"

#include <iostream>
#include <cstdint>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "../core/CommandEventProtocol.h"
#include "../core/SimulationEngine.h"
#include "../core/Logger.h"
#include "../core/OutputManager.h"

namespace MoLab {

namespace {

class IpcEventWriter {
public:
    IpcEventWriter() : running_(true), writer_thread_([this]() { writerLoop(); }) {}

    ~IpcEventWriter() {
        stop();
    }

    void emit(const nlohmann::json& json_line) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(json_line.dump());
        }
        cv_.notify_one();
    }

    void stop() {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false)) {
            return;
        }

        cv_.notify_all();
        if (writer_thread_.joinable()) {
            writer_thread_.join();
        }
    }

private:
    void writerLoop() {
        for (;;) {
            std::string line;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { return !running_.load() || !queue_.empty(); });
                if (!running_.load() && queue_.empty()) {
                    return;
                }
                line = std::move(queue_.front());
                queue_.pop_front();
            }

            std::cout << line << '\n';
            std::cout.flush();
        }
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::string> queue_;
    std::atomic<bool> running_{false};
    std::thread writer_thread_;
};

bool tryReadPositiveInterval(const nlohmann::json& payload,
                             const char* key,
                             uint64_t& target,
                             std::string* error) {
    if (!payload.contains(key)) {
        return true;
    }

    if (!payload[key].is_number_integer()) {
        if (error) {
            *error = std::string("payload.") + key + " must be a positive integer";
        }
        return false;
    }

    const auto value = payload[key].get<int64_t>();
    if (value <= 0) {
        if (error) {
            *error = std::string("payload.") + key + " must be positive";
        }
        return false;
    }

    target = static_cast<uint64_t>(value);
    return true;
}

} // namespace

std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

void applyLogLevel(Logger& logger, const std::string& log_level) {
    if (log_level == "DEBUG") {
        logger.setLogLevel(LogLevel::DEBUG);
    } else if (log_level == "INFO") {
        logger.setLogLevel(LogLevel::INFO);
    } else if (log_level == "WARNING") {
        logger.setLogLevel(LogLevel::WARNING);
    } else if (log_level == "ERROR") {
        logger.setLogLevel(LogLevel::ERR);
    } else if (log_level == "CRITICAL") {
        logger.setLogLevel(LogLevel::CRITICAL);
    }
}

bool resolveIpcThrottleSettings(const SimulationConfig& simulation_config,
                                const nlohmann::json& payload,
                                IpcThrottleSettings& settings,
                                std::string* error) {
    settings.tick_event_interval = static_cast<uint64_t>(simulation_config.ipc_tick_event_interval);
    settings.telemetry_interval_ticks = static_cast<uint64_t>(simulation_config.ipc_telemetry_interval_ticks);

    if (!payload.is_object()) {
        if (error) {
            *error = "payload must be a JSON object";
        }
        return false;
    }

    if (!tryReadPositiveInterval(payload, "tick_event_interval", settings.tick_event_interval, error)) {
        return false;
    }

    if (!tryReadPositiveInterval(payload, "telemetry_interval_ticks", settings.telemetry_interval_ticks, error)) {
        return false;
    }

    return true;
}

int runIpcStdioSession(SimulationEngine& engine, Logger& logger) {
    logger.setConsoleOutputEnabled(false);

    IpcEventWriter ipc_writer;
    std::atomic<uint64_t> event_seq{0};

    auto emit_json = [&](const nlohmann::json& json_line) {
        ipc_writer.emit(json_line);
    };

    auto emit_event = [&](const std::string& event_name, const nlohmann::json& payload = nlohmann::json::object()) {
        const uint64_t seq = event_seq.fetch_add(1) + 1;
        emit_json(buildEventJson(event_name, payload, seq));
    };

    const auto& simulation_config = ConfigManager::getInstance().getSimulationConfig();
    std::atomic<uint64_t> telemetry_interval_ticks{
        static_cast<uint64_t>(simulation_config.ipc_telemetry_interval_ticks)
    };

    logger.setStructuredSink([&](LogLevel level, const std::string& message, const std::string& component) {
        emit_event("log", {
            {"level", logLevelToString(level)},
            {"component", component},
            {"message", message}
        });

        if (level == LogLevel::ERR || level == LogLevel::CRITICAL) {
            emit_event("error", {
                {"level", logLevelToString(level)},
                {"component", component},
                {"message", message}
            });
        }
    });

    auto& output_manager = OutputManager::getInstance();
    output_manager.setRealtimeTelemetryCallback([&](const SimulationDataPoint& point, int tick) {
        const uint64_t interval = telemetry_interval_ticks.load();
        if (interval == 0 || (static_cast<uint64_t>(tick) % interval) != 0) {
            return;
        }

        emit_event("state_sample", {
            {"tick", tick},
            {"sim_time", point.time},
            {"position", {point.position_x, point.position_y, point.position_z}},
            {"velocity", {point.velocity_x, point.velocity_y, point.velocity_z}}
        });
    });

    bool engine_initialized = false;
    bool simulation_active = false;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        ParsedCommand command;
        std::string parse_error;
        if (!parseCommandJsonLine(line, command, &parse_error)) {
            emit_json(buildErrorJson("", "parse_error", parse_error));
            continue;
        }

        if (command.command == "initialize") {
            if (engine_initialized) {
                engine.shutdown();
                engine_initialized = false;
            }

            if (!engine.initialize_from_loaded_config()) {
                emit_json(buildErrorJson(command.request_id, "engine_error", "Failed to initialize simulation engine"));
                continue;
            }

            engine_initialized = true;
            output_manager.setOutputInterval(simulation_config.ipc_telemetry_interval_ticks);
            emit_json(buildAckJson(command.request_id, {{"initialized", true}}));
        } else if (command.command == "run_ticks") {
            if (!engine_initialized) {
                emit_json(buildErrorJson(command.request_id, "invalid_state", "Engine not initialized"));
                continue;
            }

            if (!command.payload.contains("count") || !command.payload["count"].is_number_integer()) {
                emit_json(buildErrorJson(command.request_id, "invalid_payload", "run_ticks requires integer payload.count"));
                continue;
            }

            IpcThrottleSettings throttle_settings;
            std::string throttle_error;
            if (!resolveIpcThrottleSettings(simulation_config, command.payload, throttle_settings, &throttle_error)) {
                emit_json(buildErrorJson(command.request_id, "invalid_payload", throttle_error));
                continue;
            }
            telemetry_interval_ticks.store(throttle_settings.telemetry_interval_ticks);
            output_manager.setOutputInterval(static_cast<int>(throttle_settings.telemetry_interval_ticks));

            const int count = command.payload["count"].get<int>();
            if (count <= 0) {
                emit_json(buildErrorJson(command.request_id, "invalid_payload", "payload.count must be positive"));
                continue;
            }

            bool run_ticks_failed = false;
            simulation_active = true;
            emit_event("simulation_started", {
                {"mode", "run_ticks"},
                {"count", count},
                {"tick_event_interval", throttle_settings.tick_event_interval},
                {"telemetry_interval_ticks", throttle_settings.telemetry_interval_ticks}
            });

            for (int i = 0; i < count; ++i) {
                engine.run_tick();
                const auto status = engine.getStatus();
                if (status.tick % throttle_settings.tick_event_interval == 0 || i == count - 1) {
                    emit_event("tick_completed", {
                        {"tick", status.tick},
                        {"sim_time", status.sim_time},
                        {"last_tick_duration", status.last_tick_duration},
                        {"compute_tick_duration_ms", status.compute_tick_duration_ms},
                        {"io_tick_duration_ms", status.io_tick_duration_ms}
                    });
                }
                if (!status.running) {
                    emit_json(buildErrorJson(command.request_id, "engine_error", "Simulation stopped during run_ticks"));
                    run_ticks_failed = true;
                    break;
                }
            }

            simulation_active = false;

            const auto status = engine.getStatus();
            emit_event("simulation_finished", {
                {"mode", "run_ticks"},
                {"tick", status.tick},
                {"sim_time", status.sim_time}
            });
            if (run_ticks_failed) {
                continue;
            }
            emit_json(buildAckJson(command.request_id, {
                {"tick", status.tick},
                {"sim_time", status.sim_time},
                {"last_tick_duration", status.last_tick_duration},
                {"compute_tick_duration_ms", status.compute_tick_duration_ms},
                {"io_tick_duration_ms", status.io_tick_duration_ms}
            }));
        } else if (command.command == "run_full") {
            if (!engine_initialized) {
                emit_json(buildErrorJson(command.request_id, "invalid_state", "Engine not initialized"));
                continue;
            }

            IpcThrottleSettings throttle_settings;
            std::string throttle_error;
            if (!resolveIpcThrottleSettings(simulation_config, command.payload, throttle_settings, &throttle_error)) {
                emit_json(buildErrorJson(command.request_id, "invalid_payload", throttle_error));
                continue;
            }
            telemetry_interval_ticks.store(throttle_settings.telemetry_interval_ticks);
            output_manager.setOutputInterval(static_cast<int>(throttle_settings.telemetry_interval_ticks));

            simulation_active = true;
            emit_event("simulation_started", {
                {"mode", "run_full"},
                {"tick_event_interval", throttle_settings.tick_event_interval},
                {"telemetry_interval_ticks", throttle_settings.telemetry_interval_ticks}
            });

            bool run_full_failed = false;
            uint64_t last_tick_event_tick = 0;
            while (true) {
                const auto pre_status = engine.getStatus();
                if (pre_status.sim_time >= simulation_config.simulation_duration ||
                    pre_status.tick >= static_cast<uint64_t>(simulation_config.max_iterations)) {
                    break;
                }

                engine.run_tick();
                const auto status = engine.getStatus();

                if (status.tick % throttle_settings.tick_event_interval == 0) {
                    emit_event("tick_completed", {
                        {"tick", status.tick},
                        {"sim_time", status.sim_time},
                        {"last_tick_duration", status.last_tick_duration},
                        {"compute_tick_duration_ms", status.compute_tick_duration_ms},
                        {"io_tick_duration_ms", status.io_tick_duration_ms}
                    });
                    last_tick_event_tick = status.tick;
                }

                if (!status.running) {
                    emit_json(buildErrorJson(command.request_id, "engine_error", "Simulation stopped during run_full"));
                    run_full_failed = true;
                    break;
                }
            }

            simulation_active = false;

            const auto status = engine.getStatus();
            if (!run_full_failed && status.tick > 0 && status.tick != last_tick_event_tick) {
                emit_event("tick_completed", {
                    {"tick", status.tick},
                    {"sim_time", status.sim_time},
                    {"last_tick_duration", status.last_tick_duration},
                    {"compute_tick_duration_ms", status.compute_tick_duration_ms},
                    {"io_tick_duration_ms", status.io_tick_duration_ms}
                });
            }

            emit_event("simulation_finished", {
                {"mode", "run_full"},
                {"success", !run_full_failed},
                {"tick", status.tick},
                {"sim_time", status.sim_time}
            });

            if (run_full_failed) {
                continue;
            }

            emit_json(buildAckJson(command.request_id, {
                {"tick", status.tick},
                {"sim_time", status.sim_time},
                {"last_tick_duration", status.last_tick_duration},
                {"compute_tick_duration_ms", status.compute_tick_duration_ms},
                {"io_tick_duration_ms", status.io_tick_duration_ms}
            }));
        } else if (command.command == "shutdown") {
            if (engine_initialized) {
                engine.shutdown();
                engine_initialized = false;
            }
            emit_event("simulation_finished", {{"mode", "shutdown"}});
            emit_json(buildAckJson(command.request_id, {{"shutdown", true}}));
        } else if (command.command == "get_status") {
            const auto status = engine.getStatus();
            emit_json(buildAckJson(command.request_id, {
                {"initialized", engine_initialized},
                {"session_running", simulation_active},
                {"engine_started", status.running},
                {"tick", status.tick},
                {"sim_time", status.sim_time},
                {"last_tick_duration", status.last_tick_duration},
                {"compute_tick_duration_ms", status.compute_tick_duration_ms},
                {"io_tick_duration_ms", status.io_tick_duration_ms}
            }));
        } else {
            emit_json(buildErrorJson(command.request_id, "unsupported_command", "Unsupported command: " + command.command));
        }
    }

    output_manager.setRealtimeTelemetryCallback(nullptr);
    logger.setStructuredSink(nullptr);
    if (engine_initialized) {
        engine.shutdown();
    }
    ipc_writer.stop();

    return 0;
}

} // namespace MoLab
