/** @file test_propulsion.cpp
 * @brief Initialization, parser, lookup and ABI regression tests.
 */
#include "propulsion_module.h"
#include "plugin_api.h"
#include "state_vector_generated.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
const std::string MOCK_PATH = std::string(PROPULSION_TEST_DATA_DIR) + "/engine_curves.csv";
const std::string HEADER = "ambient_pressure_pa,chamber_pressure_pa,mixture_ratio,thrust_n,isp_s,interpolation\n";
const std::string FIRST_ROW = "0,7000000,2.5,110000,300,linear\n";
const std::string LAST_ROW = "100000,7000000,2.5,100000,280,linear\n";

class PropulsionTest : public ::testing::Test {
protected:
    std::filesystem::path directory;
    propulsion::PropulsionModule module;
    void SetUp() override {
        const auto id = std::chrono::steady_clock::now().time_since_epoch().count();
        directory = std::filesystem::temp_directory_path() / ("propulsion-test-" + std::to_string(id));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
    }
    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
    std::string write(const std::string& content) {
        const auto path = directory / "curve.csv";
        std::ofstream out(path, std::ios::binary);
        out << content;
        return path.string();
    }
};

TEST_F(PropulsionTest, InitializesMockEngineAndReservesPhysicsInterfaces) {
    EXPECT_FALSE(module.initialized());
    EXPECT_FALSE(module.interpolate_performance(0));
    module.load_from_csv(MOCK_PATH);
    ASSERT_TRUE(module.initialized());
    ASSERT_EQ(module.curve().samples.size(), 3u);
    EXPECT_DOUBLE_EQ(module.curve().chamber_pressure_pa, 7000000);
    EXPECT_DOUBLE_EQ(module.curve().mixture_ratio, 2.5);
    EXPECT_EQ(module.curve().interpolation, propulsion::InterpolationMethod::Linear);
    EXPECT_DOUBLE_EQ(module.curve().samples[0].isp_s, 300);
    EXPECT_DOUBLE_EQ(module.curve().samples[2].thrust_n, 100000);
    EXPECT_FALSE(module.compute_thrust_n({0, 1}));
    EXPECT_FALSE(module.compute_mass_flow_kg_s({0, 1}));
}

TEST_F(PropulsionTest, InterpolatesNodesAndInteriorWithoutExtrapolation) {
    module.load_from_csv(MOCK_PATH);
    for (const auto& row : module.curve().samples) {
        const auto actual = module.interpolate_performance(row.ambient_pressure_pa);
        ASSERT_TRUE(actual);
        EXPECT_DOUBLE_EQ(actual->thrust_n, row.thrust_n);
        EXPECT_DOUBLE_EQ(actual->isp_s, row.isp_s);
    }
    const auto middle = module.interpolate_performance(25000);
    ASSERT_TRUE(middle);
    EXPECT_DOUBLE_EQ(middle->thrust_n, 107500);
    EXPECT_DOUBLE_EQ(middle->isp_s, 295);
    EXPECT_FALSE(module.interpolate_performance(-1));
    EXPECT_FALSE(module.interpolate_performance(100001));
    EXPECT_FALSE(module.interpolate_performance(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(module.interpolate_performance(std::numeric_limits<double>::infinity()));
}

TEST_F(PropulsionTest, SupportsBomCrLfWhitespaceAndScientificNotation) {
    std::string header = HEADER;
    header.back() = '\r';
    module.load_from_csv(write("\xEF\xBB\xBF" + header + "\n\r\n"
        " 0 , 7e6 , 2.5 , 1.1e5 , 300 , linear\r\n"
        "100000,7000000,2.5,100000,280,linear"));
    ASSERT_EQ(module.curve().samples.size(), 2u);
    EXPECT_DOUBLE_EQ(module.curve().samples.front().thrust_n, 110000);
}

TEST_F(PropulsionTest, RejectsMalformedTablesAndPreservesPreviousCurve) {
    module.load_from_csv(MOCK_PATH);
    const std::vector<std::string> invalid = {
        "", HEADER, HEADER + FIRST_ROW, "wrong,header\n" + FIRST_ROW + LAST_ROW,
        HEADER + FIRST_ROW + "1,7000000,2.5,100000,280\n",
        HEADER + FIRST_ROW + LAST_ROW + "200000,7000000,2.5,1,2,linear,\n",
        HEADER + FIRST_ROW + "1,7000000,2.5,,280,linear\n",
        HEADER + FIRST_ROW + "1,7000000,2.5,100000,280,\n",
        HEADER + FIRST_ROW + "1,7000000,2.5,100000,280,cubic\n",
        HEADER + FIRST_ROW + FIRST_ROW,
        HEADER + LAST_ROW + FIRST_ROW,
        HEADER + FIRST_ROW + "1,8000000,2.5,100000,280,linear\n",
        HEADER + FIRST_ROW + "1,7000000,3,100000,280,linear\n",
        HEADER + FIRST_ROW + "-1,7000000,2.5,100000,280,linear\n",
        HEADER + FIRST_ROW + "1,0,2.5,100000,280,linear\n",
        HEADER + FIRST_ROW + "1,7000000,0,100000,280,linear\n",
        HEADER + FIRST_ROW + "1,7000000,2.5,-1,280,linear\n",
        HEADER + FIRST_ROW + "1,7000000,2.5,100000,0,linear\n",
        HEADER + FIRST_ROW + "1,7000000,2.5,100000,nan,linear\n",
        HEADER + FIRST_ROW + "1,7000000,2.5,inf,280,linear\n",
        HEADER + FIRST_ROW + "1,7000000,2.5,1e999,280,linear\n",
        HEADER + FIRST_ROW + "1,7000000,2.5,100000,280s,linear\n",
        HEADER + FIRST_ROW + "1,7000000,2.5,100000,\"280\",linear\n"
    };
    for (const auto& text : invalid) {
        SCOPED_TRACE(text);
        EXPECT_THROW(module.load_from_csv(write(text)), std::runtime_error);
        ASSERT_EQ(module.curve().samples.size(), 3u);
        EXPECT_DOUBLE_EQ(module.curve().samples.back().isp_s, 280);
    }
}

TEST_F(PropulsionTest, DiagnosticsIncludePathAndPhysicalLineAndSuccessfulReloadReplacesData) {
    const auto path = write(HEADER + FIRST_ROW + "\ninvalid\n");
    try {
        module.load_from_csv(path);
        FAIL() << "Expected CSV failure";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find(path + ":4:"), std::string::npos);
    }
    EXPECT_FALSE(module.initialized());
    EXPECT_THROW(module.load_from_csv((directory / "missing.csv").string()), std::runtime_error);
    module.load_from_csv(MOCK_PATH);
    module.load_from_csv(write(HEADER + FIRST_ROW + LAST_ROW));
    EXPECT_EQ(module.curve().samples.size(), 2u);
}

using Handle = std::unique_ptr<std::remove_pointer_t<PluginHandle>, decltype(&plugin_destroy_instance)>;
Handle create() { return Handle(plugin_create_instance(), plugin_destroy_instance); }
std::string config(const std::string& path) {
    return nlohmann::json{{"engine_curves_path", path}}.dump();
}
std::vector<uint8_t> state_buffer() {
    flatbuffers::FlatBufferBuilder builder;
    const auto state = state_vector::CreateGeneralState(builder);
    state_vector::FinishGeneralStateBuffer(builder, state);
    return {builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize()};
}

TEST(PropulsionPlugin, LifecycleKeepsStateImmutableAndOutputsNeutral) {
    auto handle = create();
    ASSERT_NE(handle, nullptr);
    auto buffer = state_buffer();
    const auto original = buffer;
    PluginVector3 force{1, 2, 3}, torque{4, 5, 6};
    PluginTickData tick{};
    tick.state_buffer = buffer.data();
    tick.buffer_size = static_cast<uint32_t>(buffer.size());
    tick.delta_time = 0.01;
    tick.output_force = &force;
    tick.output_torque = &torque;
    EXPECT_EQ(plugin_tick(handle.get(), &tick), -2);
    ASSERT_EQ(plugin_configure(handle.get(), config(MOCK_PATH).c_str()), 0);
    EXPECT_EQ(plugin_tick(handle.get(), &tick), 0);
    EXPECT_FLOAT_EQ(force.x, 0); EXPECT_FLOAT_EQ(force.y, 0); EXPECT_FLOAT_EQ(force.z, 0);
    EXPECT_FLOAT_EQ(torque.x, 0); EXPECT_FLOAT_EQ(torque.y, 0); EXPECT_FLOAT_EQ(torque.z, 0);
    EXPECT_EQ(buffer, original);
    EXPECT_EQ(plugin_configure(handle.get(), "{"), -3);
    EXPECT_EQ(plugin_configure(handle.get(), config(MOCK_PATH + ".missing").c_str()), -4);
    EXPECT_EQ(plugin_tick(handle.get(), &tick), 0);
    auto other = create();
    EXPECT_EQ(plugin_tick(other.get(), &tick), -2);
}

TEST(PropulsionPlugin, HandlesInvalidConfigurationAndOptionalOutputs) {
    auto handle = create();
    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(plugin_configure(nullptr, "{}"), -1);
    EXPECT_EQ(plugin_configure(handle.get(), nullptr), -1);
    for (const auto* invalid : {"null", "[]", "{\"engine_curves_path\":4}", "{\"engine_curves_path\":null}"}) {
        EXPECT_EQ(plugin_configure(handle.get(), invalid), -3);
    }
    ASSERT_EQ(plugin_configure(handle.get(), "{}"), 0); // Deployed default fixture.
    auto buffer = state_buffer();
    PluginTickData tick{};
    tick.state_buffer = buffer.data();
    tick.buffer_size = static_cast<uint32_t>(buffer.size());
    EXPECT_EQ(plugin_tick(handle.get(), &tick), 0);
    PluginVector3 legacy{1, 2, 3}, canonical{4, 5, 6};
    tick.force_out = &legacy;
    tick.torque_out = &canonical;
    EXPECT_EQ(plugin_tick(handle.get(), &tick), 0);
    EXPECT_FLOAT_EQ(legacy.z, 0); EXPECT_FLOAT_EQ(canonical.y, 0);
    legacy = {1, 2, 3};
    tick.output_force = &canonical;
    EXPECT_EQ(plugin_tick(handle.get(), &tick), 0);
    EXPECT_FLOAT_EQ(legacy.x, 1); // Canonical output takes precedence.
    EXPECT_EQ(plugin_tick(nullptr, &tick), -1);
    EXPECT_EQ(plugin_tick(handle.get(), nullptr), -1);
    tick.delta_time = -1;
    EXPECT_EQ(plugin_tick(handle.get(), &tick), -3);
    tick.delta_time = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(plugin_tick(handle.get(), &tick), -3);
    tick.delta_time = 0;
    tick.buffer_size = 1;
    EXPECT_EQ(plugin_tick(handle.get(), &tick), -3);
    tick.buffer_size = static_cast<uint32_t>(buffer.size());
    buffer[0] = 255;
    EXPECT_EQ(plugin_tick(handle.get(), &tick), -3);
    tick.state_buffer = nullptr;
    EXPECT_EQ(plugin_tick(handle.get(), &tick), -1);
    plugin_destroy_instance(nullptr);
}
} // namespace
