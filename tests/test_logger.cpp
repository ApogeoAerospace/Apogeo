#include <gtest/gtest.h>
#include "Logger.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <thread>

/**
 * @file test_logger.cpp
 * @brief Unit tests for `Logger`.
 */

using namespace MoLab;
namespace fs = std::filesystem;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_log_file = "test_molab.log";
        if (fs::exists(test_log_file)) {
            fs::remove(test_log_file);
        }
    }

    void TearDown() override {
        if (fs::exists(test_log_file)) {
            fs::remove(test_log_file);
        }
    }

    std::string test_log_file;
};

TEST_F(LoggerTest, SingletonInstance) {
    auto& logger1 = Logger::getInstance();
    auto& logger2 = Logger::getInstance();
    EXPECT_EQ(&logger1, &logger2);
}

TEST_F(LoggerTest, SetLogFile) {
    auto& logger = Logger::getInstance();
    logger.setLogFile(test_log_file);
    logger.log(LogLevel::INFO, "Test message", "Test");

    // Log file should be created
    EXPECT_TRUE(fs::exists(test_log_file));
}

TEST_F(LoggerTest, LogLevelFiltering) {
    auto& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::WARNING);
    logger.setLogFile(test_log_file);

    // These should not be logged (below WARNING level)
    logger.log(LogLevel::DEBUG, "Debug message", "Core");
    logger.log(LogLevel::INFO, "Info message", "Core");

    // These should be logged
    logger.log(LogLevel::WARNING, "Warning message", "Core");
    logger.log(LogLevel::ERR, "Error message", "Core");
    logger.log(LogLevel::CRITICAL, "Critical message", "Core");

    // Read log file and verify only WARNING and above
    std::ifstream log_file(test_log_file);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("Warning message") != std::string::npos);
    EXPECT_TRUE(content.find("Error message") != std::string::npos);
    EXPECT_TRUE(content.find("Critical message") != std::string::npos);
    EXPECT_TRUE(content.find("Debug message") == std::string::npos);
    EXPECT_TRUE(content.find("Info message") == std::string::npos);
}

TEST_F(LoggerTest, AllLogLevels) {
    auto& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::DEBUG);
    logger.setLogFile(test_log_file);

    logger.log(LogLevel::DEBUG, "Debug level", "Test");
    logger.log(LogLevel::INFO, "Info level", "Test");
    logger.log(LogLevel::WARNING, "Warning level", "Test");
    logger.log(LogLevel::ERR, "Error level", "Test");
    logger.log(LogLevel::CRITICAL, "Critical level", "Test");

    std::ifstream log_file(test_log_file);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("[DEBUG]") != std::string::npos);
    EXPECT_TRUE(content.find("[INFO]") != std::string::npos);
    EXPECT_TRUE(content.find("[WARN]") != std::string::npos);
    EXPECT_TRUE(content.find("[ERROR]") != std::string::npos);
    EXPECT_TRUE(content.find("[CRIT]") != std::string::npos);
}

TEST_F(LoggerTest, ComponentTagging) {
    auto& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::INFO);
    logger.setLogFile(test_log_file);

    logger.log(LogLevel::INFO, "Core message", "Core");
    logger.log(LogLevel::INFO, "Physics message", "Physics");
    logger.log(LogLevel::INFO, "Plugin message", "Plugin");

    std::ifstream log_file(test_log_file);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("[Core]") != std::string::npos);
    EXPECT_TRUE(content.find("[Physics]") != std::string::npos);
    EXPECT_TRUE(content.find("[Plugin]") != std::string::npos);
}

TEST_F(LoggerTest, ThreadSafety) {
    auto& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::DEBUG);
    logger.setLogFile(test_log_file);

    // Log from multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&logger, i]() {
            for (int j = 0; j < 10; ++j) {
                logger.log(LogLevel::INFO, "Message " + std::to_string(j),
                          "Thread" + std::to_string(i));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify all messages were logged
    std::ifstream log_file(test_log_file);
    int line_count = 0;
    std::string line;
    while (std::getline(log_file, line)) {
        if (!line.empty()) {
            line_count++;
        }
    }

    EXPECT_EQ(line_count, 100);  // 10 threads * 10 messages each
}

TEST_F(LoggerTest, SetLogLevel) {
    auto& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::ERR);
    logger.setLogFile(test_log_file);

    // Initially only ERR and above
    logger.log(LogLevel::INFO, "Info 1", "Test");
    logger.log(LogLevel::ERR, "Error 1", "Test");

    // Change to DEBUG
    logger.setLogLevel(LogLevel::DEBUG);
    logger.log(LogLevel::DEBUG, "Debug 1", "Test");
    logger.log(LogLevel::INFO, "Info 2", "Test");

    std::ifstream log_file(test_log_file);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());

    // Info 1 should NOT be logged (was ERROR level at that time)
    EXPECT_TRUE(content.find("Info 1") == std::string::npos);
    // Error 1 should be logged
    EXPECT_TRUE(content.find("Error 1") != std::string::npos);
    // Debug 1 and Info 2 should be logged (changed to DEBUG)
    EXPECT_TRUE(content.find("Debug 1") != std::string::npos);
    EXPECT_TRUE(content.find("Info 2") != std::string::npos);
}

TEST_F(LoggerTest, MultipleWrites) {
    auto& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::INFO);

    // First write
    logger.setLogFile(test_log_file);
    logger.log(LogLevel::INFO, "Session 1", "Test");

    // Second write (should append)
    logger.log(LogLevel::INFO, "Session 2", "Test");

    std::ifstream log_file(test_log_file);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("Session 1") != std::string::npos);
    EXPECT_TRUE(content.find("Session 2") != std::string::npos);
}

TEST_F(LoggerTest, EmptyMessages) {
    auto& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::INFO);
    logger.setLogFile(test_log_file);

    EXPECT_NO_THROW(logger.log(LogLevel::INFO, "", "Test"));
    EXPECT_NO_THROW(logger.log(LogLevel::INFO, "Message", ""));
    EXPECT_NO_THROW(logger.log(LogLevel::INFO, "", ""));
}

TEST_F(LoggerTest, LongMessages) {
    auto& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::INFO);
    logger.setLogFile(test_log_file);

    std::string long_msg(10000, 'A');
    EXPECT_NO_THROW(logger.log(LogLevel::INFO, long_msg, "Test"));

    std::ifstream log_file(test_log_file);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find(long_msg) != std::string::npos);
}

TEST_F(LoggerTest, SpecialCharacters) {
    auto& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::INFO);
    logger.setLogFile(test_log_file);

    logger.log(LogLevel::INFO, "Special: \n\t\r", "Test");
    logger.log(LogLevel::INFO, "Unicode: 你好 мир", "Test");

    EXPECT_TRUE(fs::exists(test_log_file));
}
