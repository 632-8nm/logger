#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "MPMCQueue.hpp"
#include "Sink.hpp"

#include <atomic>
#include <format>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

enum class LogLevel { TRACE, DEBUG, INFO, WARN, ERROR, FATAL };

class Logger {
public:
	static Logger& getInstance() {
		static Logger instance;
		return instance;
	}

	void addSink(std::shared_ptr<Sink> sink) { sinks_.push_back(sink); }
	void setLevel(LogLevel level) { level_ = level; }

	template <typename... Args>
	void log(LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
		if (level < level_)
			return;

		std::string msg = std::format(fmt, std::forward<Args>(args)...);
		std::string finalMsg = formatMessage(level, msg);

		if (asyncRunning_)
			queue_.push(finalMsg);
		else {
			std::lock_guard<std::mutex> lock(mutex_);
			for (auto& sink : sinks_) sink->write(finalMsg);
		}
	}

	// 异步控制
	void startAsync() {
		asyncRunning_ = true;
		worker_ = std::thread([this] {
			std::string msg;
			while (asyncRunning_) {
				while (queue_.pop(msg)) {
					std::lock_guard<std::mutex> lock(mutex_);
					for (auto& sink : sinks_) sink->write(msg);
				}
				std::this_thread::sleep_for(std::chrono::microseconds(10));
			}
			while (queue_.pop(msg)) {
				std::lock_guard<std::mutex> lock(mutex_);
				for (auto& sink : sinks_) sink->write(msg);
			}
		});
	}
	void stopAsync() {
		if (asyncRunning_) {
			asyncRunning_ = false;
			queue_.stop();
			if (worker_.joinable())
				worker_.join();
		}
	}

private:
	Logger() {}
	~Logger() { stopAsync(); }
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	std::string formatMessage(LogLevel level, const std::string& msg) {
		std::ostringstream oss;

		auto now = std::chrono::system_clock::now();
		auto t = std::chrono::system_clock::to_time_t(now);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
					  now.time_since_epoch())
				  % 1000;
		std::tm tm;
#ifdef _WIN32
		localtime_s(&tm, &t);
#else
		localtime_r(&t, &tm);
#endif

		// 输出时间
		oss << "[" << std::put_time(&tm, "%F %T") << "." << std::setw(3)
			<< std::setfill('0') << ms.count() << "]";

		// 输出日志级别
		static const char* levelStr[] = {"TRACE", "DEBUG", "INFO",
										 "WARN",  "ERROR", "FATAL"};
		oss << "[" << levelStr[(int)level] << "] ";

		// 输出日志消息
		oss << msg;

		return oss.str();
	}

private:
	std::vector<std::shared_ptr<Sink>> sinks_;
	std::mutex mutex_;
	LogLevel level_ = LogLevel::TRACE;

	MPMCQueue<std::string> queue_;
	std::thread worker_;
	std::atomic<bool> asyncRunning_{false};
};

#define LOG_TRACE(fmt, ...) \
	Logger::getInstance().log(LogLevel::TRACE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) \
	Logger::getInstance().log(LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) \
	Logger::getInstance().log(LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) \
	Logger::getInstance().log(LogLevel::WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) \
	Logger::getInstance().log(LogLevel::ERROR, fmt, ##__VA_ARGS__)

#endif // LOGGER_H