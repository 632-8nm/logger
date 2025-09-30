#ifndef SERVER_HPP
#define SERVER_HPP

#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

enum class ServerMode { THREAD_PER_CONN, POLL, EPOLL };

class LogServer {
public:
	LogServer(int port, ServerMode mode = ServerMode::THREAD_PER_CONN)
		: port_(port), mode_(mode) {}

	void start() {
		running_ = true;

		serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
		if (serverFd_ < 0) {
			perror("socket");
			return;
		}

		int opt = 1;
		setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port_);

		if (bind(serverFd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
			perror("bind");
			return;
		}
		if (listen(serverFd_, 10) < 0) {
			perror("listen");
			return;
		}

		std::cout << "[LogServer] Listening on port " << port_ << std::endl;
		printLocalIPs();

		if (mode_ == ServerMode::THREAD_PER_CONN) {
			acceptThread_ = std::thread([this] { acceptLoop(); });
		} else if (mode_ == ServerMode::POLL) {
			workerThread_ = std::thread([this] { pollLoop(); });
		} else if (mode_ == ServerMode::EPOLL) {
			workerThread_ = std::thread([this] { epollLoop(); });
		}
	}

	void stop() {
		running_ = false;
		if (acceptThread_.joinable())
			acceptThread_.join();
		if (workerThread_.joinable())
			workerThread_.join();
		close(serverFd_);
	}

private:
	// ----------- 线程 per connection -----------
	void acceptLoop() {
		while (running_) {
			sockaddr_in clientAddr{};
			socklen_t len = sizeof(clientAddr);
			int clientFd = accept(serverFd_, (sockaddr*)&clientAddr, &len);
			if (clientFd < 0)
				continue;

			std::thread([this, clientFd] { handleClient(clientFd); }).detach();
		}
	}

	void handleClient(int clientFd) {
		char buf[1024];
		while (true) {
			ssize_t n = read(clientFd, buf, sizeof(buf) - 1);
			if (n <= 0)
				break;
			buf[n] = '\0';
			std::cout << "[LogServer] " << buf << std::endl;
		}
		close(clientFd);
	}

	// ----------- poll 实现 -----------
	void pollLoop() {
		std::vector<pollfd> fds;
		fds.push_back({serverFd_, POLLIN, 0});

		while (running_) {
			int ready = poll(fds.data(), fds.size(), 1000);
			if (ready <= 0)
				continue;

			for (size_t i = 0; i < fds.size(); ++i) {
				if (fds[i].revents & POLLIN) {
					if (fds[i].fd == serverFd_) {
						int clientFd = accept(serverFd_, nullptr, nullptr);
						if (clientFd >= 0) {
							fds.push_back({clientFd, POLLIN, 0});
						}
					} else {
						char buf[1024];
						ssize_t n = read(fds[i].fd, buf, sizeof(buf) - 1);
						if (n <= 0) {
							close(fds[i].fd);
							fds.erase(fds.begin() + i);
							--i;
						} else {
							buf[n] = '\0';
							std::cout << "[LogServer] " << buf << std::endl;
						}
					}
				}
			}
		}
	}

	// ----------- epoll 实现 -----------
	void epollLoop() {
#ifdef __linux__
#include <sys/epoll.h>
		int epfd = epoll_create1(0);
		if (epfd < 0) {
			perror("epoll_create1");
			return;
		}

		epoll_event ev{}, events[16];
		ev.events = EPOLLIN;
		ev.data.fd = serverFd_;
		epoll_ctl(epfd, EPOLL_CTL_ADD, serverFd_, &ev);

		while (running_) {
			int nfds = epoll_wait(epfd, events, 16, 1000);
			if (nfds < 0)
				continue;

			for (int i = 0; i < nfds; ++i) {
				if (events[i].data.fd == serverFd_) {
					int clientFd = accept(serverFd_, nullptr, nullptr);
					if (clientFd >= 0) {
						fcntl(clientFd, F_SETFL, O_NONBLOCK);
						epoll_event cev{};
						cev.events = EPOLLIN;
						cev.data.fd = clientFd;
						epoll_ctl(epfd, EPOLL_CTL_ADD, clientFd, &cev);
					}
				} else {
					char buf[1024];
					ssize_t n = read(events[i].data.fd, buf, sizeof(buf) - 1);
					if (n <= 0) {
						close(events[i].data.fd);
					} else {
						buf[n] = '\0';
						std::cout << "[LogServer] " << buf << std::endl;
					}
				}
			}
		}

		close(epfd);
#else
		std::cerr << "[LogServer] epoll not supported on this platform\n";
#endif
	}

	void printLocalIPs() {
		struct ifaddrs* ifAddrStruct = nullptr;
		getifaddrs(&ifAddrStruct);

		std::cout << "Available local IP addresses:\n";
		for (struct ifaddrs* ifa = ifAddrStruct; ifa != nullptr;
			 ifa = ifa->ifa_next) {
			if (!ifa->ifa_addr)
				continue;
			if (ifa->ifa_addr->sa_family == AF_INET) {
				void* addrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
				char addrBuf[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, addrPtr, addrBuf, sizeof(addrBuf));
				std::cout << "  " << ifa->ifa_name << " -> " << addrBuf << ":"
						  << port_ << "\n";
			}
		}

		if (ifAddrStruct)
			freeifaddrs(ifAddrStruct);
	}

private:
	int port_;
	int serverFd_{-1};
	std::atomic<bool> running_{false};
	ServerMode mode_;

	std::thread acceptThread_;
	std::thread workerThread_;
};

#endif // LOG_SERVER_HPP
