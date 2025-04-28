#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>
#include <ctime>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    auto GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }
        return std::chrono::system_clock::now();
    }

    std::string GetTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &t_c);
#else
        localtime_r(&t_c, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%F %T");
        return oss.str();
    }

    std::string GetFileTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &t_c);
#else
        localtime_r(&t_c, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y_%m_%d");
        return oss.str();
    }

    Logger() = default;
    Logger(const Logger&) = delete;

    std::mutex mutex_;
    std::ofstream current_stream_;
    std::string current_filename_;
    std::optional<std::chrono::system_clock::time_point> manual_ts_;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    template<class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto file_timestamp = GetFileTimeStamp();
        std::string filename = "/var/log/sample_log_" + file_timestamp + ".log";

        if (current_filename_ != filename) {
            current_stream_.close();
            current_stream_.open(filename, std::ios::app);
            current_filename_ = filename;
        }

        if (current_stream_.is_open()) {
            current_stream_ << GetTimeStamp() << ": ";
            (current_stream_ << ... << args);
            current_stream_ << std::endl;
        }
    }

    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard<std::mutex> lock(mutex_);
        manual_ts_ = ts;
    }
};