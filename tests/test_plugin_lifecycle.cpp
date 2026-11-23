#include <gtest/gtest.h>
#include "PluginManager.h"
#include "ConfigManager.h"
#include "InitialStateLoader.h"
#include <fstream>
#include <nlohmann/json.hpp>

TEST(PluginLifecycle, LoadsRunsMeasuresAndUnloadsConfiguredPlugin) {
    const std::string file_name = "plugin-lifecycle.json";
    const nlohmann::json configuration = {
        {"plugins", {{{"name", "propulsion"}, {"library_path", APOGEO_PROPULSION_PLUGIN},
                      {"type", 1}, {"parameters", {{"use_host_logger", true},
                        {"engine_curves_path", APOGEO_SOURCE_DIR "/plugins/propulsion/data/engine_curves.csv"}}}}}}
    };
    { std::ofstream file(file_name); file << configuration; }
    ASSERT_TRUE(Apogeo::ConfigManager::getInstance().loadConfig(file_name));
    Apogeo::PluginManager manager;
    ASSERT_TRUE(manager.load_plugins_from_config());
    EXPECT_EQ(manager.get_plugin_count(), 1U);
    ASSERT_EQ(manager.get_loaded_plugin_names().size(), 1U);
    EXPECT_EQ(manager.get_loaded_plugin_names()[0], "propulsion");
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(Apogeo::InitialStateLoader::create_state_from_json(
        builder, APOGEO_SOURCE_DIR "/data/defaults/default_state.json"));
    std::vector<uint8_t> buffer(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
    manager.run_simulation_cycle(buffer, 0.01);
    const auto metrics = manager.get_plugin_metrics();
    ASSERT_EQ(metrics.size(), 1U);
    EXPECT_EQ(metrics[0].name, "propulsion");
    EXPECT_EQ(metrics[0].execution_count, 1U);
    EXPECT_GE(metrics[0].total_execution_time, 0);
    EXPECT_GE(metrics[0].average_execution_time, 0);
    manager.shutdown();
    EXPECT_EQ(manager.get_plugin_count(), 0U);
    EXPECT_TRUE(manager.get_loaded_plugin_names().empty());
    EXPECT_TRUE(manager.get_plugin_metrics().empty());
    std::remove(file_name.c_str());
}

TEST(PluginLifecycle, MissingDynamicLibraryDoesNotRegisterAnInstance) {
    Apogeo::PluginManager manager;
    EXPECT_FALSE(manager.load_plugin("not-an-existing-plugin", PluginType::PARALLEL_PHYSICS_CALCULATOR));
    EXPECT_EQ(manager.get_plugin_count(), 0U);
}
