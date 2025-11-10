#include <gtest/gtest.h>
#include "../src/core/Logger.h"
#include <fstream>
#include <filesystem>
#include <sstream>

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

TEST_F(LoggerTest, InitializeWithFile) {
    auto& logger = Logger::getInstance();
    logger.initialize(test_log_file, LogLevel::INFO);
    
    // Log file should be created
    EXPECT_TRUE(fs::exists(test_log_file));
}

TEST_F(LoggerTest, LogLevelFiltering) {
    auto& logger = Logger::getInstance();
    logger.initialize(test_log_file, LogLevel::WARNING);
    
    // These should not be logged (below WARNING level)
    logger.log(LogLevel::DEBUG, "Core", "Debug message");
    logger.log(LogLevel::INFO, "Core", "Info message");
    
    // These should be logged
    logger.log(LogLevel::WARNING, "Core", "Warning message");
    logger.log(LogLevel::ERROR, "Core", "Error message");
    logger.log(LogLevel::CRITICAL, "Core", "Critical message");
    
    logger.shutdown();
    
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
    logger.initialize(test_log_file, LogLevel::DEBUG);
    
    logger.log(LogLevel::DEBUG, "Test", "Debug level");
    logger.log(LogLevel::INFO, "Test", "Info level");
    logger.log(LogLevel::WARNING, "Test", "Warning level");
    logger.log(LogLevel::ERROR, "Test", "Error level");
    logger.log(LogLevel::CRITICAL, "Test", "Critical level");
    
    logger.shutdown();
    
    std::ifstream log_file(test_log_file);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(content.find("[DEBUG]") != std::string::npos);
    EXPECT_TRUE(content.find("[INFO]") != std::string::npos);
    EXPECT_TRUE(content.find("[WARNING]") != std::string::npos);
    EXPECT_TRUE(content.find("[ERROR]") != std::string::npos);
    EXPECT_TRUE(content.find("[CRITICAL]") != std::string::npos);
}

TEST_F(LoggerTest, ComponentTagging) {
    auto& logger = Logger::getInstance();
    logger.initialize(test_log_file, LogLevel::INFO);
    
    logger.log(LogLevel::INFO, "Core", "Core message");
    logger.log(LogLevel::INFO, "Physics", "Physics message");
    logger.log(LogLevel::INFO, "Plugin", "Plugin message");
    
    logger.shutdown();
    
    std::ifstream log_file(test_log_file);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(content.find("[Core]") != std::string::npos);
    EXPECT_TRUE(content.find("[Physics]") != std::string::npos);
    EXPECT_TRUE(content.find("[Plugin]") != std::string::npos);
}

TEST_F(LoggerTest, ThreadSafety) {
    auto& logger = Logger::getInstance();
    logger.initialize(test_log_file, LogLevel::DEBUG);
    
    // Log from multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&logger, i]() {
            for (int j = 0; j < 10; ++j) {
                logger.log(LogLevel::INFO, "Thread" + std::to_string(i), 
                          "Message " + std::to_string(j));
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    logger.shutdown();
    
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
    logger.initialize(test_log_file, LogLevel::ERROR);
    
    // Initially only ERROR and above
    logger.log(LogLevel::INFO, "Test", "Info 1");
    logger.log(LogLevel::ERROR, "Test", "Error 1");
    
    // Change to DEBUG
    logger.setLogLevel(LogLevel::DEBUG);
    logger.log(LogLevel::DEBUG, "Test", "Debug 1");
    logger.log(LogLevel::INFO, "Test", "Info 2");
    
    logger.shutdown();
    
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

TEST_F(LoggerTest, ShutdownAndRestart) {
    auto& logger = Logger::getInstance();
    
    // First session
    logger.initialize(test_log_file, LogLevel::INFO);
    logger.log(LogLevel::INFO, "Test", "Session 1");
    logger.shutdown();
    
    // Second session (should append)
    logger.initialize(test_log_file, LogLevel::INFO);
    logger.log(LogLevel::INFO, "Test", "Session 2");
    logger.shutdown();
    
    std::ifstream log_file(test_log_file);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(content.find("Session 1") != std::string::npos);
    EXPECT_TRUE(content.find("Session 2") != std::string::npos);
}

TEST_F(LoggerTest, EmptyMessages) {
    auto& logger = Logger::getInstance();
    logger.initialize(test_log_file, LogLevel::INFO);
    
    EXPECT_NO_THROW(logger.log(LogLevel::INFO, "Test", ""));
    EXPECT_NO_THROW(logger.log(LogLevel::INFO, "", "Message"));
    EXPECT_NO_THROW(logger.log(LogLevel::INFO, "", ""));
    
    logger.shutdown();
}

TEST_F(LoggerTest, LongMessages) {
    auto& logger = Logger::getInstance();
    logger.initialize(test_log_file, LogLevel::INFO);
    
    std::string long_msg(10000, 'A');
    EXPECT_NO_THROW(logger.log(LogLevel::INFO, "Test", long_msg));
    
    logger.shutdown();
    
    std::ifstream log_file(test_log_file);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(content.find(long_msg) != std::string::npos);
}

TEST_F(LoggerTest, SpecialCharacters) {
    auto& logger = Logger::getInstance();
    logger.initialize(test_log_file, LogLevel::INFO);
    
    logger.log(LogLevel::INFO, "Test", "Special: \n\t\r");
    logger.log(LogLevel::INFO, "Test", "Unicode: 你好 мир");
    
    logger.shutdown();
    
    EXPECT_TRUE(fs::exists(test_log_file));
}
