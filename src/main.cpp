#include "Logger.hpp"
#include "Sink.hpp"

#include <thread>

int main() {
	auto& logger = Logger::getInstance();
	logger.addSink(std::make_shared<ConsoleSink>());
	logger.addSink(std::make_shared<FileSink>("log", 1024 * 1024, 5));
	logger.addSink(std::make_shared<TCPSink>("10.211.55.10", 9000));
	logger.setLevel(LogLevel::TRACE);
	logger.startAsync();

	constexpr int thread_num = 4;
	constexpr int message_num = 100;
	std::vector<std::thread> threads;
	for (int t = 0; t < thread_num; ++t) {
		threads.emplace_back([t, message_num] {
			for (int i = 0; i < message_num; i++) {
				LOG_INFO("Thread {} logging INFO {}", t, i);
				LOG_ERROR("Thread {} logging ERROR {}", t, i);
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		});
	}

	for (auto& th : threads) th.join();
	logger.stopAsync();
	return 0;
}
