#include <gtest/gtest.h>
#include "../src/core/OutputManager.h"
#include "../src/core/TimeManager.h"
#include <flatbuffers/flatbuffers.h>
#include <filesystem>
#include <fstream>

using namespace MoLab;
namespace fs = std::filesystem;

class OutputManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_dir = "test_output";
        fs::create_directories(test_dir);
        auto &out = OutputManager::getInstance();
        out.setOutputDirectory(test_dir);
        out.setOutputFormats(true, true, false);
        out.setOutputInterval(1);
    }
    void TearDown() override
    {
        if (fs::exists(test_dir))
        {
            fs::remove_all(test_dir);
        }
    }
    std::string test_dir;
};

TEST_F(OutputManagerTest, InitializeAndFinalize)
{
    auto &out = OutputManager::getInstance();
    EXPECT_NO_THROW(out.initializeOutput("unittest"));
    EXPECT_NO_THROW(out.finalizeOutput());
}

TEST_F(OutputManagerTest, RecordStateCSV)
{
    auto &out = OutputManager::getInstance();
    out.setOutputFormats(true, false, false);
    out.initializeOutput("csvtest");

    flatbuffers::FlatBufferBuilder builder;

    // Structs FlatBuffers (NO CreateX)
    state_vector::Vec3 position(1.0f, 2.0f, 3.0f);
    state_vector::Vec3 velocity(0.1f, 0.2f, 0.3f);
    state_vector::Quaternion orientation(0.0f, 0.0f, 0.0f, 1.0f);
    state_vector::Vec3 gravity(0.0f, 0.0f, -9.81f);
    state_vector::Vec3 wind_speed(0.0f, 0.0f, 0.0f);

    // Escalares
    float atm_density = 1.225f;
    float atm_pressure = 101325.0f;
    float atm_temperature = 288.15f;
    float utc = 0.0f;
    float time = 0.0f;

    // Table principal
    auto state = state_vector::CreateGeneralState(
        builder,
        &position,
        &velocity,
        &orientation,
        atm_density,
        atm_pressure,
        atm_temperature,
        &gravity,
        utc,
        time,
        &wind_speed);

    builder.Finish(state);
    std::vector<uint8_t> buf(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
    out.recordState(buf, 0.0, 0.0, 0);
    out.finalizeOutput();

    bool found = false;
    for (const auto &entry : fs::directory_iterator(test_dir))
    {
        if (entry.path().extension() == ".csv")
            found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(OutputManagerTest, RecordStateJSON)
{
    auto &out = OutputManager::getInstance();
    out.setOutputFormats(false, true, false);
    out.initializeOutput("jsontest");

    flatbuffers::FlatBufferBuilder builder;

    // Structs FlatBuffers (NO CreateX)
    state_vector::Vec3 position(1.0f, 2.0f, 3.0f);
    state_vector::Vec3 velocity(0.1f, 0.2f, 0.3f);
    state_vector::Quaternion orientation(0.0f, 0.0f, 0.0f, 1.0f);
    state_vector::Vec3 gravity(0.0f, 0.0f, -9.81f);
    state_vector::Vec3 wind_speed(0.0f, 0.0f, 0.0f);

    // Escalares
    float atm_density = 1.225f;
    float atm_pressure = 101325.0f;
    float atm_temperature = 288.15f;
    float utc = 0.0f;
    float time = 0.0f;

    // Table principal
    auto state = state_vector::CreateGeneralState(
        builder,
        &position,
        &velocity,
        &orientation,
        atm_density,
        atm_pressure,
        atm_temperature,
        &gravity,
        utc,
        time,
        &wind_speed);

    builder.Finish(state);
    std::vector<uint8_t> buf(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
    out.recordState(buf, 0.0, 0.0, 0);
    out.finalizeOutput();

    bool found = false;
    for (const auto &entry : fs::directory_iterator(test_dir))
    {
        if (entry.path().extension() == ".json")
            found = true;
    }
    EXPECT_TRUE(found);
}
