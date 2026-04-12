#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <functional>
#include <vector>
#include <deque>

/**
 * @file Logger.h
 * @brief Centralized logging system for MoLab.
 */

namespace MoLab {

/**
 * @enum LogLevel
 * @brief Severity levels supported by the logger.
 */
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERR = 3,
    CRITICAL = 4
};

/**
 * @class Logger
 * @brief Singleton logger with console and file output.
 */
class Logger {
public:
    /*
     * @struct LogEntry
     * @brief Represents a single log entry for buffering and structured sinks.
     */
    struct LogEntry {
        LogLevel level;
        std::string message;
        std::string component;
    };

    /**
     * @brief Gets the global logger instance.
     * @return Unique reference to logger.
     */
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    /**
     * @brief Sets the minimum severity level to emit.
     * @param level New minimum level.
     */
    void setLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_level_ = level;
    }

    /**
     * @brief Configures an output log file in append mode.
     * @param filename Log file path.
     */
    void setLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        log_file_path_ = filename;
        if (log_file_.is_open()) {
            log_file_.close();
        }
        if (!log_file_path_.empty()) {
            log_file_.open(log_file_path_, std::ios::app);
        }
    }

    /**
     * @brief Enables or disables console/stdout log emission.
     * @param enabled If `true`, logs are written to console.
     */
    void setConsoleOutputEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        console_output_enabled_ = enabled;
    }

    /**
     * @brief Sets an optional structured sink callback for log events.
     * @param sink Callback receiving `(level, message, component)`.
     */
    void setStructuredSink(std::function<void(LogLevel, const std::string&, const std::string&)> sink) {
        std::lock_guard<std::mutex> lock(mutex_);
        structured_sink_ = std::move(sink);
    }

    /**
     * @brief Gets a snapshot of recent buffered log entries.
     * @return Vector of recent log entries.
     */
    std::vector<LogEntry> getBufferedLogsSnapshot() {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::vector<LogEntry>(recent_logs_.begin(), recent_logs_.end());
    }

    /**
     * @brief Closes the active log file and clears configured path.
     */
    void closeLogFile() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (log_file_.is_open()) {
            log_file_.close();
        }
        log_file_path_.clear();
    }

    /**
     * @brief Emits a formatted message by level and component.
     * @param level Message severity level.
     * @param message Message text.
     * @param component Source component name.
     */
    void log(LogLevel level, const std::string& message, const std::string& component = "") {
        if (level < current_level_) {
            return;
        }

        std::function<void(LogLevel, const std::string&, const std::string&)> structured_sink;

        {
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

            recent_logs_.push_back(LogEntry{level, message, component});
            if (recent_logs_.size() > max_buffered_logs_) {
                recent_logs_.pop_front();
            }

            // Output to console
            if (console_output_enabled_) {
                std::cout << log_line << std::endl;
            }

            // Output to file if available
            if (!log_file_path_.empty()) {
                if (!log_file_.is_open()) {
                    log_file_.open(log_file_path_, std::ios::app);
                }
                if (log_file_.is_open()) {
                    log_file_ << log_line << std::endl;
                    log_file_.flush();
                    log_file_.close();
                }
            }

            structured_sink = structured_sink_;
        }

        if (structured_sink) {
            try {
                structured_sink(level, message, component);
            } catch (...) {
                // Structured sink does not break normal logging path.
            }
        }
    }

    /**
     * @brief Emits a `DEBUG`-level message.
     * @param message Message text.
     * @param component Source component.
     */
    void debug(const std::string& message, const std::string& component = "") {
        log(LogLevel::DEBUG, message, component);
    }

    /**
     * @brief Emits an `INFO`-level message.
     * @param message Message text.
     * @param component Source component.
     */
    void info(const std::string& message, const std::string& component = "") {
        log(LogLevel::INFO, message, component);
    }

    /**
     * @brief Emits a `WARNING`-level message.
     * @param message Message text.
     * @param component Source component.
     */
    void warning(const std::string& message, const std::string& component = "") {
        log(LogLevel::WARNING, message, component);
    }

    /**
     * @brief Emits an `ERROR`-level message.
     * @param message Message text.
     * @param component Source component.
     */
    void error(const std::string& message, const std::string& component = "") {
        log(LogLevel::ERR, message, component);
    }

    /**
     * @brief Emits a `CRITICAL`-level message.
     * @param message Message text.
     * @param component Source component.
     */
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

    /**
     * @brief Converts a log level to a string representation.
     * @param level Level to convert.
     * @return Readable level string.
     */
    std::string levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARN";
            case LogLevel::ERR: return "ERROR";
            case LogLevel::CRITICAL: return "CRIT";
            default: return "UNKNOWN";
        }
    }

    LogLevel current_level_;
    std::ofstream log_file_;
    std::string log_file_path_;
    bool console_output_enabled_ = true;
    std::function<void(LogLevel, const std::string&, const std::string&)> structured_sink_;
    std::deque<LogEntry> recent_logs_;
    static constexpr std::size_t max_buffered_logs_ = 256;
    std::mutex mutex_;
};

/** @brief Convenience macro for `DEBUG` level. */
#define LOG_DEBUG(msg, component) MoLab::Logger::getInstance().debug(msg, component)
/** @brief Convenience macro for `INFO` level. */
#define LOG_INFO(msg, component) MoLab::Logger::getInstance().info(msg, component)
/** @brief Convenience macro for `WARNING` level. */
#define LOG_WARNING(msg, component) MoLab::Logger::getInstance().warning(msg, component)
/** @brief Convenience macro for `ERROR` level. */
#define LOG_ERROR(msg, component) MoLab::Logger::getInstance().error(msg, component)
/** @brief Convenience macro for `CRITICAL` level. */
#define LOG_CRITICAL(msg, component) MoLab::Logger::getInstance().critical(msg, component)

} // namespace MoLab
