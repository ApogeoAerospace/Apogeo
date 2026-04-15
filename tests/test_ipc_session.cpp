#include <gtest/gtest.h>
#include "ipc/IpcSession.h"

using namespace MoLab;

TEST(IpcSessionTest, ResolveThrottleSettingsUsesConfigDefaults) {
    SimulationConfig config{};
    config.ipc_tick_event_interval = 40;
    config.ipc_telemetry_interval_ticks = 8;

    IpcThrottleSettings settings;
    std::string error;
    EXPECT_TRUE(resolveIpcThrottleSettings(config, nlohmann::json::object(), settings, &error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(settings.tick_event_interval, 40u);
    EXPECT_EQ(settings.telemetry_interval_ticks, 8u);
}

TEST(IpcSessionTest, ResolveThrottleSettingsAppliesPayloadOverrides) {
    SimulationConfig config{};
    config.ipc_tick_event_interval = 50;
    config.ipc_telemetry_interval_ticks = 5;

    IpcThrottleSettings settings;
    const auto payload = nlohmann::json{{"tick_event_interval", 12}, {"telemetry_interval_ticks", 3}};

    EXPECT_TRUE(resolveIpcThrottleSettings(config, payload, settings));
    EXPECT_EQ(settings.tick_event_interval, 12u);
    EXPECT_EQ(settings.telemetry_interval_ticks, 3u);
}

TEST(IpcSessionTest, ResolveThrottleSettingsRejectsInvalidOverrides) {
    SimulationConfig config{};
    config.ipc_tick_event_interval = 50;
    config.ipc_telemetry_interval_ticks = 5;

    IpcThrottleSettings settings;
    std::string error;

    EXPECT_FALSE(resolveIpcThrottleSettings(config, nlohmann::json{{"tick_event_interval", 0}}, settings, &error));
    EXPECT_FALSE(error.empty());

    error.clear();
    EXPECT_FALSE(resolveIpcThrottleSettings(config, nlohmann::json{{"telemetry_interval_ticks", "fast"}}, settings, &error));
    EXPECT_FALSE(error.empty());
}
