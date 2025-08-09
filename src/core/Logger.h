#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>

namespace MoLab {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void setLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_level_ = level;
    }

    void setLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (log_file_.is_open()) {
            log_file_.close();
        }
        log_file_.open(filename, std::ios::app);
    }

    void log(LogLevel level, const std::string& message, const std::string& component = "") {
        if (level < current_level_) return;

        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        ss << " [" << levelToString(level) << "]";
        
        if (!component.empty()) {
            ss << " [" << component << "]";
        }
        
        ss << " " << message;

        std::string log_line = ss.str();
        
        // Output to console
        std::cout << log_line << std::endl;
        
        // Output to file if available
        if (log_file_.is_open()) {
            log_file_ << log_line << std::endl;
            log_file_.flush();
        }
    }

    // Convenience methods
    void debug(const std::string& message, const std::string& component = "") {
        log(LogLevel::DEBUG, message, component);
    }

    void info(const std::string& message, const std::string& component = "") {
        log(LogLevel::INFO, message, component);
    }

    void warning(const std::string& message, const std::string& component = "") {
        log(LogLevel::WARNING, message, component);
    }

    void error(const std::string& message, const std::string& component = "") {
        log(LogLevel::ERROR, message, component);
    }

    void critical(const std::string& message, const std::string& component = "") {
        log(LogLevel::CRITICAL, message, component);
    }

private:
    Logger() : current_level_(LogLevel::INFO) {}
    ~Logger() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::CRITICAL: return "CRIT";
            default: return "UNKNOWN";
        }
    }

    LogLevel current_level_;
    std::ofstream log_file_;
    std::mutex mutex_;
};

// Convenience macros
#define LOG_DEBUG(msg, component) MoLab::Logger::getInstance().debug(msg, component)
#define LOG_INFO(msg, component) MoLab::Logger::getInstance().info(msg, component)
#define LOG_WARNING(msg, component) MoLab::Logger::getInstance().warning(msg, component)
#define LOG_ERROR(msg, component) MoLab::Logger::getInstance().error(msg, component)
#define LOG_CRITICAL(msg, component) MoLab::Logger::getInstance().critical(msg, component)

} // namespace MoLab
