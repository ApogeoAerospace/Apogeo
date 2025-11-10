#include <gtest/gtest.h>
#include "../src/core/OutputManager.h"
#include "../src/core/TimeManager.h"
#include <flatbuffers/flatbuffers.h>
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
    auto pos = state_vector::CreateVector3(builder, 1,2,3);
    auto vel = state_vector::CreateVector3(builder, 0.1,0.2,0.3);
    auto ori = state_vector::CreateQuaternion(builder, 0,0,0,1);
    auto ang = state_vector::CreateVector3(builder, 0,0,0);
    auto grav = state_vector::CreateVector3(builder, 0,0,-9.81);
    auto state = state_vector::CreateGeneralState(
        builder, pos, vel, ori, ang, 1000.0, 1.225, 101325, 288.15, grav, 0.0
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
    auto pos = state_vector::CreateVector3(builder, 1,2,3);
    auto vel = state_vector::CreateVector3(builder, 0.1,0.2,0.3);
    auto ori = state_vector::CreateQuaternion(builder, 0,0,0,1);
    auto ang = state_vector::CreateVector3(builder, 0,0,0);
    auto grav = state_vector::CreateVector3(builder, 0,0,-9.81);
    auto state = state_vector::CreateGeneralState(
        builder, pos, vel, ori, ang, 1000.0, 1.225, 101325, 288.15, grav, 0.0
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
