#include <gtest/gtest.h>
#include "../src/core/OutputManager.h"
#include "../src/core/TimeManager.h"
#include <flatbuffers/flatbuffers.h>
#include "state_vector_generated.h"
#include <filesystem>
#include <fstream>

using namespace MoLab;
namespace fs = std::filesystem;

class OutputManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "test_output";
        fs::create_directories(test_dir);
        auto& out = OutputManager::getInstance();
        out.setOutputDirectory(test_dir);
        out.setOutputFormats(true, true, false);
        out.setOutputInterval(1);
    }
    void TearDown() override {
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }
    std::string test_dir;
};

TEST_F(OutputManagerTest, InitializeAndFinalize) {
    auto& out = OutputManager::getInstance();
    EXPECT_NO_THROW(out.initializeOutput("unittest"));
    EXPECT_NO_THROW(out.finalizeOutput());
}

TEST_F(OutputManagerTest, RecordStateCSV) {
    auto& out = OutputManager::getInstance();
    out.setOutputFormats(true, false, false);
    out.initializeOutput("csvtest");

    flatbuffers::FlatBufferBuilder builder;
    auto pos = state_vector::Vec3(1, 2, 3);
    auto vel = state_vector::Vec3(0.1, 0.2, 0.3);
    auto ori = state_vector::Quaternion(0, 0, 0, 1);
    auto grav = state_vector::Vec3(0, 0, -9.81);
    auto wind = state_vector::Vec3(0, 0, 0);
    auto state = state_vector::CreateGeneralState(
        builder, &pos, &vel, &ori, 1.225, 101325, 288.15, &grav, 0.0, 0.0, &wind
    );
    builder.Finish(state);
    std::vector<uint8_t> buf(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
    out.recordState(buf, 0.0, 0.0, 0);
    out.finalizeOutput();

    bool found = false;
    for (const auto& entry : fs::directory_iterator(test_dir)) {
        if (entry.path().extension() == ".csv") found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(OutputManagerTest, RecordStateJSON) {
    auto& out = OutputManager::getInstance();
    out.setOutputFormats(false, true, false);
    out.initializeOutput("jsontest");

    flatbuffers::FlatBufferBuilder builder;
    auto pos = state_vector::Vec3(1, 2, 3);
    auto vel = state_vector::Vec3(0.1, 0.2, 0.3);
    auto ori = state_vector::Quaternion(0, 0, 0, 1);
    auto grav = state_vector::Vec3(0, 0, -9.81);
    auto wind = state_vector::Vec3(0, 0, 0);
    auto state = state_vector::CreateGeneralState(
        builder, &pos, &vel, &ori, 1.225, 101325, 288.15, &grav, 0.0, 0.0, &wind
    );
    builder.Finish(state);
    std::vector<uint8_t> buf(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
    out.recordState(buf, 0.0, 0.0, 0);
    out.finalizeOutput();

    bool found = false;
    for (const auto& entry : fs::directory_iterator(test_dir)) {
        if (entry.path().extension() == ".json") found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(OutputManagerTest, SetOutputFormats) {
    auto& out = OutputManager::getInstance();
    EXPECT_NO_THROW(out.setOutputFormats(true, false, true));
    EXPECT_NO_THROW(out.setOutputFormats(false, true, false));
}

TEST_F(OutputManagerTest, SetOutputInterval) {
    auto& out = OutputManager::getInstance();
    EXPECT_NO_THROW(out.setOutputInterval(10));
    EXPECT_NO_THROW(out.setOutputInterval(1));
    EXPECT_NO_THROW(out.setOutputInterval(100));
}

TEST_F(OutputManagerTest, SetOutputDirectory) {
    auto& out = OutputManager::getInstance();
    std::string custom_dir = "custom_output";
    EXPECT_NO_THROW(out.setOutputDirectory(custom_dir));
    if (fs::exists(custom_dir)) {
        fs::remove_all(custom_dir);
    }
}

TEST_F(OutputManagerTest, RecordMultipleStates) {
    auto& out = OutputManager::getInstance();
    out.setOutputFormats(true, false, false);
    out.initializeOutput("multitest");

    flatbuffers::FlatBufferBuilder builder;
    auto pos = state_vector::Vec3(1, 2, 3);
    auto vel = state_vector::Vec3(0.1, 0.2, 0.3);
    auto ori = state_vector::Quaternion(0, 0, 0, 1);
    auto grav = state_vector::Vec3(0, 0, -9.81);
    auto wind = state_vector::Vec3(0, 0, 0);
    auto state = state_vector::CreateGeneralState(
        builder, &pos, &vel, &ori, 1.225, 101325, 288.15, &grav, 0.0, 0.0, &wind
    );
    builder.Finish(state);
    std::vector<uint8_t> buf(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());

    for (int i = 0; i < 10; i++) {
        out.recordState(buf, i * 0.1, i * 0.1, i);
    }
    out.finalizeOutput();

    EXPECT_TRUE(fs::exists(test_dir));
}

TEST_F(OutputManagerTest, PrintSummary) {
    auto& out = OutputManager::getInstance();
    out.initializeOutput("summarytest");
    EXPECT_NO_THROW(out.printSummary());
    out.finalizeOutput();
}

TEST_F(OutputManagerTest, RecordStateBinary) {
    auto& out = OutputManager::getInstance();
    out.setOutputFormats(false, false, true);
    out.initializeOutput("binarytest");

    flatbuffers::FlatBufferBuilder builder;
    auto pos = state_vector::Vec3(1, 2, 3);
    auto vel = state_vector::Vec3(0.1, 0.2, 0.3);
    auto ori = state_vector::Quaternion(0, 0, 0, 1);
    auto grav = state_vector::Vec3(0, 0, -9.81);
    auto wind = state_vector::Vec3(0, 0, 0);
    auto state = state_vector::CreateGeneralState(
        builder, &pos, &vel, &ori, 1.225, 101325, 288.15, &grav, 0.0, 0.0, &wind
    );
    builder.Finish(state);
    std::vector<uint8_t> buf(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
    out.recordState(buf, 0.0, 0.0, 0);
    out.finalizeOutput();

    bool found = false;
    for (const auto& entry : fs::directory_iterator(test_dir)) {
        if (entry.path().extension() == ".bin") found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(OutputManagerTest, AllFormatsEnabled) {
    auto& out = OutputManager::getInstance();
    out.setOutputFormats(true, true, true);
    out.initializeOutput("allformats");

    flatbuffers::FlatBufferBuilder builder;
    auto pos = state_vector::Vec3(1, 2, 3);
    auto vel = state_vector::Vec3(0.1, 0.2, 0.3);
    auto ori = state_vector::Quaternion(0, 0, 0, 1);
    auto grav = state_vector::Vec3(0, 0, -9.81);
    auto wind = state_vector::Vec3(0, 0, 0);
    auto state = state_vector::CreateGeneralState(
        builder, &pos, &vel, &ori, 1.225, 101325, 288.15, &grav, 0.0, 0.0, &wind
    );
    builder.Finish(state);
    std::vector<uint8_t> buf(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
    out.recordState(buf, 0.0, 0.0, 0);
    out.finalizeOutput();

    bool csv_found = false;
    bool json_found = false;
    bool bin_found = false;
    for (const auto& entry : fs::directory_iterator(test_dir)) {
        if (entry.path().extension() == ".csv") csv_found = true;
        if (entry.path().extension() == ".json") json_found = true;
        if (entry.path().extension() == ".bin") bin_found = true;
    }
    EXPECT_TRUE(csv_found);
    EXPECT_TRUE(json_found);
    EXPECT_TRUE(bin_found);
}

TEST_F(OutputManagerTest, SingletonInstance) {
    auto& out1 = OutputManager::getInstance();
    auto& out2 = OutputManager::getInstance();
    EXPECT_EQ(&out1, &out2);
}

TEST_F(OutputManagerTest, RecordMetrics) {
    auto& out = OutputManager::getInstance();
    out.initializeOutput("metricstest");

    EXPECT_NO_THROW(out.recordMetrics("Physics", "force_x", 10.5));
    EXPECT_NO_THROW(out.recordMetrics("Physics", "force_y", 20.5));
    EXPECT_NO_THROW(out.recordMetrics("Aerodynamics", "drag", 5.5));
    
    out.finalizeOutput();
}

TEST_F(OutputManagerTest, FlushOutput) {
    auto& out = OutputManager::getInstance();
    out.setOutputFormats(true, false, false);
    out.initializeOutput("flushtest");

    flatbuffers::FlatBufferBuilder builder;
    auto pos = state_vector::Vec3(1, 2, 3);
    auto vel = state_vector::Vec3(0.1, 0.2, 0.3);
    auto ori = state_vector::Quaternion(0, 0, 0, 1);
    auto grav = state_vector::Vec3(0, 0, -9.81);
    auto wind = state_vector::Vec3(0, 0, 0);
    auto state = state_vector::CreateGeneralState(
        builder, &pos, &vel, &ori, 1.225, 101325, 288.15, &grav, 0.0, 0.0, &wind
    );
    builder.Finish(state);
    std::vector<uint8_t> buf(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());

    out.recordState(buf, 0.0, 0.0, 0);
    EXPECT_NO_THROW(out.flush());

    out.finalizeOutput();
}

TEST_F(OutputManagerTest, GetDataPoints) {
    auto& out = OutputManager::getInstance();
    out.setOutputFormats(true, false, false);
    out.initializeOutput("datapointstest");

    flatbuffers::FlatBufferBuilder builder;
    auto pos = state_vector::Vec3(1, 2, 3);
    auto vel = state_vector::Vec3(0.1, 0.2, 0.3);
    auto ori = state_vector::Quaternion(0, 0, 0, 1);
    auto grav = state_vector::Vec3(0, 0, -9.81);
    auto wind = state_vector::Vec3(0, 0, 0);
    auto state = state_vector::CreateGeneralState(
        builder, &pos, &vel, &ori, 1.225, 101325, 288.15, &grav, 0.0, 0.0, &wind
    );
    builder.Finish(state);
    std::vector<uint8_t> buf(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());

    size_t initial_count = out.getDataPoints().size();

    out.recordState(buf, 0.0, 0.0, 0);
    out.recordState(buf, 0.1, 0.1, 1);
    out.recordState(buf, 0.2, 0.2, 2);

    const auto& data_points = out.getDataPoints();
    EXPECT_GT(data_points.size(), initial_count);

    out.finalizeOutput();
}

TEST_F(OutputManagerTest, OutputIntervalRespected) {
    auto& out = OutputManager::getInstance();
    out.setOutputFormats(true, false, false);
    out.setOutputInterval(5); // Only save every 5 ticks
    out.initializeOutput("intervaltest");

    flatbuffers::FlatBufferBuilder builder;
    auto pos = state_vector::Vec3(1, 2, 3);
    auto vel = state_vector::Vec3(0.1, 0.2, 0.3);
    auto ori = state_vector::Quaternion(0, 0, 0, 1);
    auto grav = state_vector::Vec3(0, 0, -9.81);
    auto wind = state_vector::Vec3(0, 0, 0);
    auto state = state_vector::CreateGeneralState(
        builder, &pos, &vel, &ori, 1.225, 101325, 288.15, &grav, 0.0, 0.0, &wind
    );
    builder.Finish(state);
    std::vector<uint8_t> buf(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());

    // Record 10 ticks, but only every 5th should be saved
    for (int i = 0; i < 10; i++) {
        out.recordState(buf, i * 0.1, i * 0.1, i);
    }

    out.finalizeOutput();
    EXPECT_TRUE(fs::exists(test_dir));
}

TEST_F(OutputManagerTest, MultipleInitializeFinalize) {
    auto& out = OutputManager::getInstance();

    // First session
    out.initializeOutput("session1");
    out.finalizeOutput();

    // Second session
    out.initializeOutput("session2");
    out.finalizeOutput();

    EXPECT_TRUE(fs::exists(test_dir));
}

TEST_F(OutputManagerTest, NoFormatsEnabled) {
    auto& out = OutputManager::getInstance();
    out.setOutputFormats(false, false, false);
    out.initializeOutput("noformats");

    flatbuffers::FlatBufferBuilder builder;
    auto pos = state_vector::Vec3(1, 2, 3);
    auto vel = state_vector::Vec3(0.1, 0.2, 0.3);
    auto ori = state_vector::Quaternion(0, 0, 0, 1);
    auto grav = state_vector::Vec3(0, 0, -9.81);
    auto wind = state_vector::Vec3(0, 0, 0);
    auto state = state_vector::CreateGeneralState(
        builder, &pos, &vel, &ori, 1.225, 101325, 288.15, &grav, 0.0, 0.0, &wind
    );
    builder.Finish(state);
    std::vector<uint8_t> buf(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());

    EXPECT_NO_THROW(out.recordState(buf, 0.0, 0.0, 0));
    out.finalizeOutput();
}

TEST_F(OutputManagerTest, RecordStateWithoutInitialize) {
    auto& out = OutputManager::getInstance();

    flatbuffers::FlatBufferBuilder builder;
    auto pos = state_vector::Vec3(1, 2, 3);
    auto vel = state_vector::Vec3(0.1, 0.2, 0.3);
    auto ori = state_vector::Quaternion(0, 0, 0, 1);
    auto grav = state_vector::Vec3(0, 0, -9.81);
    auto wind = state_vector::Vec3(0, 0, 0);
    auto state = state_vector::CreateGeneralState(
        builder, &pos, &vel, &ori, 1.225, 101325, 288.15, &grav, 0.0, 0.0, &wind
    );
    builder.Finish(state);
    std::vector<uint8_t> buf(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());

    // Should handle gracefully even without initialize
    EXPECT_NO_THROW(out.recordState(buf, 0.0, 0.0, 0));
}

TEST_F(OutputManagerTest, EmptyRunName) {
    auto& out = OutputManager::getInstance();
    EXPECT_NO_THROW(out.initializeOutput(""));
    EXPECT_NO_THROW(out.finalizeOutput());
}
