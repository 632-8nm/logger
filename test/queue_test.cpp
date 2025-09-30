#include "../include/MPMCQueue.hpp" // 自己的队列

#include <boost/lockfree/queue.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

const int NUM_THREADS = 1;
const int NUM_ITEMS = 100000;

template <typename QueueType>
double benchmarkQueue(QueueType& queue) {
	auto start = std::chrono::high_resolution_clock::now();

	// 生产者
	std::vector<std::thread> producers;
	for (int t = 0; t < NUM_THREADS; ++t) {
		producers.emplace_back([&queue]() {
			for (int i = 0; i < NUM_ITEMS; ++i) {
				queue.push(i);
			}
		});
	}

	// 消费者
	std::vector<std::thread> consumers;
	for (int t = 0; t < NUM_THREADS; ++t) {
		consumers.emplace_back([&queue]() {
			int value;
			for (int i = 0; i < NUM_ITEMS; ++i) {
				while (!queue.pop(value)) {
					std::this_thread::yield(); // 队列空时让出CPU
				}
			}
		});
	}

	for (auto& th : producers) th.join();
	for (auto& th : consumers) th.join();

	auto end = std::chrono::high_resolution_clock::now();
	double sec = std::chrono::duration<double>(end - start).count();
	return sec;
}

int main() {
	// 1️⃣ 自己的 MPMC 队列
	MPMCQueue<int> myQueue(NUM_THREADS * NUM_ITEMS);
	double t1 = benchmarkQueue(myQueue);
	std::cout << "My MPMCQueueLog time: " << t1 << " s\n";

	// 2️⃣ Boost lockfree 队列
	boost::lockfree::queue<int> boostQueue(NUM_THREADS * NUM_ITEMS);
	double t2 = benchmarkQueue(boostQueue);
	std::cout << "Boost lockfree queue time: " << t2 << " s\n";

	return 0;
}
