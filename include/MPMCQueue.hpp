#ifndef MPMC_QUEUE_LOG_HPP
#define MPMC_QUEUE_LOG_HPP

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T>
class MPMCQueue {
public:
	explicit MPMCQueue(size_t capacity = 1024)
		: capacity_(capacity), stop_(false), freeCount_(capacity) {}

	// 生产者 push 阻塞到有空槽
	void push(const T& item) {
		// 等待空槽
		std::unique_lock<std::mutex> lockFree(mutexFree_);
		condNotFull_.wait(lockFree, [this] { return freeCount_ > 0 || stop_; });
		if (stop_)
			return;
		--freeCount_; // 消耗一个空槽
		lockFree.unlock();

		// 写入数据队列
		{
			std::lock_guard<std::mutex> lockData(mutexData_);
			queueData_.push(item);
		}
		condNotEmpty_.notify_one();
	}

	// 消费者 pop 阻塞到有数据
	bool pop(T& item) {
		std::unique_lock<std::mutex> lockData(mutexData_);
		condNotEmpty_.wait(lockData,
						   [this] { return !queueData_.empty() || stop_; });
		if (queueData_.empty())
			return false;

		item = queueData_.front();
		queueData_.pop();
		lockData.unlock();

		// 释放一个空槽
		{
			std::lock_guard<std::mutex> lockFree(mutexFree_);
			++freeCount_;
		}
		condNotFull_.notify_one();
		return true;
	}

	// 停止队列，唤醒所有阻塞线程
	void stop() {
		stop_ = true;
		condNotEmpty_.notify_all();
		condNotFull_.notify_all();
	}

	// 返回当前数据队列大小（仅参考）
	size_t size() {
		std::lock_guard<std::mutex> lock(mutexData_);
		return queueData_.size();
	}

private:
	std::queue<T> queueData_;		// 数据队列
	size_t capacity_;				// 最大容量
	std::atomic<size_t> freeCount_; // 空槽计数

	std::mutex mutexData_; // 数据队列锁
	std::mutex mutexFree_; // 空槽锁
	std::condition_variable condNotEmpty_;
	std::condition_variable condNotFull_;
	std::atomic<bool> stop_;
};

#endif // MPMC_QUEUE_LOG_H
