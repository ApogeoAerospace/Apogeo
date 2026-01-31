#include <gtest/gtest.h>
#include "../src/core/OutputManager.h"
#include "../src/core/TimeManager.h"
#include "state_vector_generated.h"
#include <filesystem>
#include <fstream>

using namespace MoLab;
namespace fs = std::filesystem;

static std::vector<uint8_t> make_state_buffer() {
  flatbuffers::FlatBufferBuilder builder;

  auto pos = state_vector::Vec3(10.0f, 20.0f, 30.0f);
  auto vel = state_vector::Vec3(1.0f, 2.0f, 3.0f);
  auto quat = state_vector::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
  auto ang = state_vector::Vec3(0.1f, 0.2f, 0.3f);
  auto wind = state_vector::Vec3(5.0f, 0.0f, 0.0f);
  auto grav = state_vector::Vec3(0.0f, -9.81f, 0.0f);

  state_vector::GeneralStateBuilder gs(builder);
  gs.add_sim_time(0.0f);
  gs.add_dt(0.01f);
  gs.add_position(&pos);
  gs.add_velocity(&vel);
  gs.add_orientation(&quat);
  gs.add_angular_velocity(&ang);
  gs.add_atm_density(1.225f);
  gs.add_atm_pressure(101325.0f);
  gs.add_atm_temperature(288.15f);
  gs.add_gravity(&grav);
  gs.add_wind_velocity(&wind);
  auto off = gs.Finish();
  builder.Finish(off);

  const uint8_t* buf = builder.GetBufferPointer();
  size_t sz = builder.GetSize();
  return std::vector<uint8_t>(buf, buf + sz);
}

class OutputManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    fs::create_directories("test_output");
    // Initialize TimeManager for UTC stamps
    TimeManager::getInstance().initialize();
    auto& om = OutputManager::getInstance();
    om.setOutputDirectory("test_output");
    om.setOutputFormats(true, true, false); // CSV + JSON
    om.setOutputInterval(1);
    om.initializeOutput("unittest_run");
  }

  void TearDown() override {
    auto& om = OutputManager::getInstance();
    om.finalizeOutput();
    if (fs::exists("test_output")) {
      fs::remove_all("test_output");
    }
  }
};

TEST_F(OutputManagerTest, RecordSingleStateWritesCSVAndJSON) {
  auto& om = OutputManager::getInstance();
  auto buffer = make_state_buffer();

  double sim_time = 0.01;
  double utc_time = TimeManager::getInstance().getCurrentRealTimeUTC();
  int tick = 1;

  ASSERT_NO_THROW(om.recordState(buffer, sim_time, utc_time, tick));

  om.finalizeOutput();

  // Verify files exist
  bool csv_found = false, json_found = false;
  for (auto& p : fs::directory_iterator("test_output")) {
    const auto name = p.path().filename().string();
    if (name.find(".csv") != std::string::npos) csv_found = true;
    if (name.find(".json") != std::string::npos) json_found = true;
  }
  EXPECT_TRUE(csv_found);
  EXPECT_TRUE(json_found);
}

TEST_F(OutputManagerTest, CSVContainsBufferFieldsDirectly) {
  auto& om = OutputManager::getInstance();
  auto buffer = make_state_buffer();

  double sim_time = 0.02;
  double utc_time = TimeManager::getInstance().getCurrentRealTimeUTC();
  int tick = 2;

  om.recordState(buffer, sim_time, utc_time, tick);
  om.finalizeOutput();

  // Find CSV file
  std::string csv_path;
  for (auto& p : fs::directory_iterator("test_output")) {
    if (p.path().extension() == ".csv") {
      csv_path = p.path().string();
      break;
    }
  }
  ASSERT_FALSE(csv_path.empty());

  std::ifstream csv(csv_path);
  ASSERT_TRUE(csv.is_open());
  std::string line;
  std::string last_line;
  while (std::getline(csv, line)) {
    last_line = line;
  }
  csv.close();

  // Last data line should include our values (position, velocity, orientation, atm, gravity, wind_velocity)
  // Simple substring checks (avoiding strict formatting dependencies)
  EXPECT_NE(last_line.find("10"), std::string::npos);    // position_x
  EXPECT_NE(last_line.find("20"), std::string::npos);    // position_y
  EXPECT_NE(last_line.find("30"), std::string::npos);    // position_z
  EXPECT_NE(last_line.find("1"), std::string::npos);     // velocity_x
  EXPECT_NE(last_line.find("2"), std::string::npos);     // velocity_y
  EXPECT_NE(last_line.find("3"), std::string::npos);     // velocity_z
  EXPECT_NE(last_line.find("1.225"), std::string::npos); // atm_density
  EXPECT_NE(last_line.find("101325"), std::string::npos);// atm_pressure
  EXPECT_NE(last_line.find("288.15"), std::string::npos);// atm_temperature
  EXPECT_NE(last_line.find("-9.81"), std::string::npos); // gravity_y
  EXPECT_NE(last_line.find("5"), std::string::npos);     // wind_velocity_x
}

TEST_F(OutputManagerTest, JSONContainsBufferFieldsDirectly) {
  auto& om = OutputManager::getInstance();
  auto buffer = make_state_buffer();

  double sim_time = 0.03;
  double utc_time = TimeManager::getInstance().getCurrentRealTimeUTC();
  int tick = 3;

  om.recordState(buffer, sim_time, utc_time, tick);
  om.finalizeOutput();

  // Find JSON file
  std::string json_path;
  for (auto& p : fs::directory_iterator("test_output")) {
    if (p.path().extension() == ".json") {
      json_path = p.path().string();
      break;
    }
  }
  ASSERT_FALSE(json_path.empty());

  std::ifstream jf(json_path);
  ASSERT_TRUE(jf.is_open());
  std::stringstream buffer_ss;
  buffer_ss << jf.rdbuf();
  jf.close();

  auto json_str = buffer_ss.str();
  // Check presence of values and keys
  EXPECT_NE(json_str.find("\"position\""), std::string::npos);
  EXPECT_NE(json_str.find("\"velocity\""), std::string::npos);
  EXPECT_NE(json_str.find("\"orientation\""), std::string::npos);
  EXPECT_NE(json_str.find("\"atmosphere\""), std::string::npos);
  EXPECT_NE(json_str.find("\"gravity\""), std::string::npos);
  EXPECT_NE(json_str.find("\"wind\""), std::string::npos);

  EXPECT_NE(json_str.find("10"), std::string::npos);
  EXPECT_NE(json_str.find("20"), std::string::npos);
  EXPECT_NE(json_str.find("30"), std::string::npos);
  EXPECT_NE(json_str.find("1.225"), std::string::npos);
  EXPECT_NE(json_str.find("101325"), std::string::npos);
  EXPECT_NE(json_str.find("288.15"), std::string::npos);
  EXPECT_NE(json_str.find("-9.81"), std::string::npos);
  EXPECT_NE(json_str.find("5"), std::string::npos);
}
