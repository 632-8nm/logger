#include "Server.hpp"

int main() {
	// 1. 传统：每连接一个线程
	// LogServer server1(9000, ServerMode::THREAD_PER_CONN);
	// server1.start();

	// 2. poll 模式
	LogServer server2(9000, ServerMode::POLL);
	server2.start();

	// 3. epoll 模式（Linux Only）
	// LogServer server3(9000, ServerMode::EPOLL);
	// server3.start();

	std::this_thread::sleep_for(std::chrono::minutes(10));
}
