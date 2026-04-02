#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>

/**
 * @file Logger.h
 * @brief Sistema de logging centralizado para MoLab.
 */

namespace MoLab {

/**
 * @enum LogLevel
 * @brief Niveles de severidad soportados por el logger.
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
 * @brief Logger singleton con salida a consola y archivo.
 */
class Logger {
public:
    /**
     * @brief Obtiene la instancia global del logger.
     * @return Referencia única al logger.
     */
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    /**
     * @brief Define el nivel mínimo de severidad a emitir.
     * @param level Nuevo nivel mínimo.
     */
    void setLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_level_ = level;
    }

    /**
     * @brief Configura un archivo de salida para logs en modo append.
     * @param filename Ruta del archivo de log.
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
     * @brief Cierra el archivo de log activo y limpia la ruta configurada.
     */
    void closeLogFile() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (log_file_.is_open()) {
            log_file_.close();
        }
        log_file_path_.clear();
    }

    /**
     * @brief Emite un mensaje formateado según nivel y componente.
     * @param level Nivel de severidad del mensaje.
     * @param message Texto del mensaje.
     * @param component Nombre del componente emisor.
     */
    void log(LogLevel level, const std::string& message, const std::string& component = "") {
        if (level < current_level_) {
            return;
        }

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
    }

    /**
     * @brief Emite un mensaje de nivel `DEBUG`.
     * @param message Texto del mensaje.
     * @param component Componente emisor.
     */
    void debug(const std::string& message, const std::string& component = "") {
        log(LogLevel::DEBUG, message, component);
    }

    /**
     * @brief Emite un mensaje de nivel `INFO`.
     * @param message Texto del mensaje.
     * @param component Componente emisor.
     */
    void info(const std::string& message, const std::string& component = "") {
        log(LogLevel::INFO, message, component);
    }

    /**
     * @brief Emite un mensaje de nivel `WARNING`.
     * @param message Texto del mensaje.
     * @param component Componente emisor.
     */
    void warning(const std::string& message, const std::string& component = "") {
        log(LogLevel::WARNING, message, component);
    }

    /**
     * @brief Emite un mensaje de nivel `ERROR`.
     * @param message Texto del mensaje.
     * @param component Componente emisor.
     */
    void error(const std::string& message, const std::string& component = "") {
        log(LogLevel::ERR, message, component);
    }

    /**
     * @brief Emite un mensaje de nivel `CRITICAL`.
     * @param message Texto del mensaje.
     * @param component Componente emisor.
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
     * @brief Convierte un nivel de log a representación textual.
     * @param level Nivel a convertir.
     * @return Cadena legible del nivel.
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
    std::mutex mutex_;
};

/** @brief Macro de conveniencia para nivel `DEBUG`. */
#define LOG_DEBUG(msg, component) MoLab::Logger::getInstance().debug(msg, component)
/** @brief Macro de conveniencia para nivel `INFO`. */
#define LOG_INFO(msg, component) MoLab::Logger::getInstance().info(msg, component)
/** @brief Macro de conveniencia para nivel `WARNING`. */
#define LOG_WARNING(msg, component) MoLab::Logger::getInstance().warning(msg, component)
/** @brief Macro de conveniencia para nivel `ERROR`. */
#define LOG_ERROR(msg, component) MoLab::Logger::getInstance().error(msg, component)
/** @brief Macro de conveniencia para nivel `CRITICAL`. */
#define LOG_CRITICAL(msg, component) MoLab::Logger::getInstance().critical(msg, component)

} // namespace MoLab
