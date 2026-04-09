#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <mutex>

class Logger {
public:
    enum class Level { DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3 };

private:
    static Level currentLevel;
    static bool enableFileOutput;
    static std::string logFilePath;
    static std::ofstream logFile;
    static std::mutex logMutex;

    static std::string getLevelString(Level level) {
        switch (level) {
            case Level::DEBUG: return "[DEBUG]";
            case Level::INFO: return "[INFO]";
            case Level::WARNING: return "[WARNING]";
            case Level::ERROR: return "[ERROR]";
            default: return "[UNKNOWN]";
        }
    }

    static std::string getCurrentTimeString() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    static void writeLog(Level level, const std::string& message,
                        const std::string& function = "", int line = 0) {
        if (level < currentLevel) return;
        std::lock_guard<std::mutex> lock(logMutex);
        std::stringstream logEntry;
        logEntry << getCurrentTimeString() << " " << getLevelString(level) << " ";
        if (!function.empty() && line > 0) {
            logEntry << "[" << function << ":" << line << "] ";
        }
        logEntry << message;
        std::cout << logEntry.str() << std::endl;
        if (enableFileOutput && logFile.is_open()) {
            logFile << logEntry.str() << std::endl;
            logFile.flush();
        }
    }

public:
    static void setLevel(Level level) {
        std::lock_guard<std::mutex> lock(logMutex);
        currentLevel = level;
    }

    static bool enableFileLogging(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(logMutex);
        if (logFile.is_open()) logFile.close();
        logFile.open(filePath, std::ios::app);
        if (logFile.is_open()) {
            logFilePath = filePath;
            enableFileOutput = true;
            return true;
        }
        return false;
    }

    static void disableFileLogging() {
        std::lock_guard<std::mutex> lock(logMutex);
        enableFileOutput = false;
        if (logFile.is_open()) logFile.close();
    }

    static Level getLevel() { return currentLevel; }

    static void log(Level level, const std::string& message,
                   const std::string& function = "", int line = 0) {
        writeLog(level, message, function, line);
    }

    static void info(const std::string& message,
                    const std::string& function = "", int line = 0) {
        writeLog(Level::INFO, message, function, line);
    }

    static void warning(const std::string& message,
                       const std::string& function = "", int line = 0) {
        writeLog(Level::WARNING, message, function, line);
    }

    static void error(const std::string& message,
                     const std::string& function = "", int line = 0) {
        writeLog(Level::ERROR, message, function, line);
    }

    static void debug(const std::string& message,
                     const std::string& function = "", int line = 0) {
        writeLog(Level::DEBUG, message, function, line);
    }

    static void network(const std::string& operation, const std::string& details) {
        std::stringstream msg;
        msg << "[NETWORK] " << operation << " - " << details;
        writeLog(Level::DEBUG, msg.str());
    }

    static void stats(const std::string& statName, size_t value) {
        std::stringstream msg;
        msg << "[STATS] " << statName << ": " << value;
        writeLog(Level::INFO, msg.str());
    }
};

Logger::Level Logger::currentLevel = Logger::Level::INFO;
bool Logger::enableFileOutput = false;
std::string Logger::logFilePath = "";
std::ofstream Logger::logFile;
std::mutex Logger::logMutex;

#define LOG_INFO(msg) Logger::info(msg, __FUNCTION__, __LINE__)
#define LOG_WARNING(msg) Logger::warning(msg, __FUNCTION__, __LINE__)
#define LOG_ERROR(msg) Logger::error(msg, __FUNCTION__, __LINE__)
#define LOG_DEBUG(msg) Logger::debug(msg, __FUNCTION__, __LINE__)
#define LOG_NETWORK(op, details) Logger::network(op, details)
#define LOG_STATS(name, value) Logger::stats(name, value)

#endif // LOGGER_H
