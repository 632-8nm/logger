#ifndef SINK_H
#define SINK_H

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

class Sink {
public:
	virtual void write(const std::string& msg) = 0;
	virtual ~Sink() = default;
};

// 控制台Sink
class ConsoleSink : public Sink {
public:
	void write(const std::string& msg) override {
		std::cout << msg << std::endl;
	}
};

// 文件Sink，支持大小+日期滚动 + 保留N个文件
class FileSink : public Sink {
public:
	FileSink(const std::string& basename, size_t maxSize = 1024 * 1024,
			 size_t maxFiles = 5)
		: basename_(basename), maxSize_(maxSize), maxFiles_(maxFiles) {
		currentDate_ = getCurrentDate();
		openNewFile();
	}

	void write(const std::string& msg) override {
		std::lock_guard<std::mutex> lock(mutex_);
		auto today = getCurrentDate();
		if (today != currentDate_) {
			currentDate_ = today;
			fileIndex_ = 0;
			openNewFile();
		}
		if (file_.tellp() >= maxSize_) {
			++fileIndex_;
			openNewFile();
		}
		file_ << msg << std::endl;
	}

private:
	std::string getCurrentDate() {
		auto now = std::chrono::system_clock::now();
		auto t = std::chrono::system_clock::to_time_t(now);
		std::tm tm;
#ifdef _WIN32
		localtime_s(&tm, &t);
#else
		localtime_r(&t, &tm);
#endif
		char buf[16];
		std::strftime(buf, sizeof(buf), "%Y%m%d", &tm);
		return std::string(buf);
	}

	void openNewFile() {
		if (file_.is_open())
			file_.close();
		std::ostringstream oss;
		oss << basename_ << "_" << currentDate_;
		if (fileIndex_ > 0)
			oss << "_" << fileIndex_;
		oss << ".log";
		file_.open(oss.str(), std::ios::out | std::ios::trunc);

		recentFiles_.push_back(oss.str());
		if (recentFiles_.size() > maxFiles_) {
			std::remove(recentFiles_.front().c_str());
			recentFiles_.pop_front();
		}
	}

private:
	std::string basename_;
	size_t maxSize_;
	size_t maxFiles_;
	std::ofstream file_;
	std::mutex mutex_;
	std::string currentDate_;
	int fileIndex_ = 0;
	std::deque<std::string> recentFiles_;
};

// 客户端Sink，发送日志到远程TCP服务器
class TCPSink : public Sink {
public:
	TCPSink(const std::string& ip, uint16_t port)
		: serverIp_(ip), serverPort_(port), stopFlag_(false) {
		worker_ = std::thread([this] { this->run(); });
	}

	~TCPSink() override {
		stopFlag_ = true;
		cv_.notify_all();
		if (worker_.joinable())
			worker_.join();
		if (sock_ >= 0)
			close(sock_);
	}

	void write(const std::string& msg) override {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			queue_.push_back(msg);
		}
		cv_.notify_one();
	}

private:
	void run() {
		while (!stopFlag_) {
			if (!connectServer()) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}

			std::unique_lock<std::mutex> lock(mutex_);
			cv_.wait(lock, [this] { return !queue_.empty() || stopFlag_; });

			while (!queue_.empty()) {
				const std::string& msg = queue_.front();
				if (!sendMessage(msg)) {
					close(sock_);
					sock_ = -1;
					break; // 失败就重新连接
				}
				queue_.pop_front();
			}
		}
	}

	bool connectServer() {
		if (sock_ >= 0)
			return true;

		sock_ = socket(AF_INET, SOCK_STREAM, 0);
		if (sock_ < 0)
			return false;

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(serverPort_);
		inet_pton(AF_INET, serverIp_.c_str(), &addr.sin_addr);

		if (connect(sock_, (sockaddr*)&addr, sizeof(addr)) < 0) {
			close(sock_);
			sock_ = -1;
			return false;
		}
		return true;
	}

	bool sendMessage(const std::string& msg) {
		const char* data = msg.c_str();
		size_t len = msg.size();
		ssize_t sent = send(sock_, data, len, 0);
		return sent == (ssize_t)len;
	}

private:
	std::string serverIp_;
	uint16_t serverPort_;
	int sock_ = -1;

	std::deque<std::string> queue_;
	std::mutex mutex_;
	std::condition_variable cv_;
	std::atomic<bool> stopFlag_;
	std::thread worker_;
};

#endif // SINK_H