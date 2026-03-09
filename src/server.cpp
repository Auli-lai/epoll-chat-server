#include "server.h"
#include "network.h"
#include "protocol.h"

#include <iostream>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <algorithm>

// 信号处理全局变量
static volatile sig_atomic_t g_running = 1;
static void handle_sigint(int) { g_running = 0; }

ChatServer::ChatServer(int port) : port_(port), running_(1) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_sigint);

    server_fd_ = create_listener_socket(port);
    if (server_fd_ < 0) {
        throw std::runtime_error("Failed to create listener socket");
    }

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to create epoll");
    }

    // 监听 Server FD
    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev);

    // 监听 Stdin
    if (!set_nonblocking(STDIN_FILENO)) {
        throw std::runtime_error("Failed to set stdin non-blocking");
    }
    ev.data.fd = STDIN_FILENO;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, STDIN_FILENO, &ev);

    std::cout << "Server started on port " << port_ << std::endl;
}

ChatServer::~ChatServer() {
    for (const auto& [fd, _] : clients_) {
        close(fd);
    }
    close(epoll_fd_);
    close(server_fd_);
    std::cout << "Server shutdown complete." << std::endl;
}

void ChatServer::run() {
    std::vector<struct epoll_event> events(100);

    while (running_) {
        int nfds = epoll_wait(epoll_fd_, events.data(), events.size(), 1000); // 1s timeout for heartbeat check
        
        if (nfds < 0) {
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait error: " << strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == server_fd_) {
                handle_accept(server_fd_, epoll_fd_);
            } else if (fd == STDIN_FILENO) {
                handle_stdin(epoll_fd_);
            } else {
                handle_client_data(fd, epoll_fd_);
            }
        }

        check_timeouts();
        running_ = g_running; // 更新运行状态
    }
}

void ChatServer::handle_accept(int server_fd, int epoll_fd) {
    while (true) {
        struct sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);

        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::cerr << "accept error: " << strerror(errno) << std::endl;
            break;
        }

        if (!set_nonblocking(client_fd)) {
            close(client_fd);
            continue;
        }

        clients_[client_fd] = {};
        clients_[client_fd].last_active = std::chrono::steady_clock::now();

        struct epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        std::cout << "New client connected: " << ip_str << ":" << ntohs(client_addr.sin_port) 
                  << " (FD: " << client_fd << ")" << std::endl;
    }
}

void ChatServer::handle_client_data(int client_fd, int epoll_fd) {
    char buf[1024];
    bool has_data = false;

    while (true) {
        ssize_t n = read(client_fd, buf, sizeof(buf));
        if (n > 0) {
            has_data = true;
            clients_[client_fd].recv_state.buffer.append(buf, n);
        } else if (n == 0) {
            // 正常断开
            cleanup_client(client_fd, epoll_fd, "disconnected");
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::cerr << "read error on fd " << client_fd << ": " << strerror(errno) << std::endl;
            cleanup_client(client_fd, epoll_fd, "read error");
            return;
        }
    }

    if (has_data) {
        clients_[client_fd].last_active = std::chrono::steady_clock::now();
        
        auto results = parse_messages(clients_[client_fd].recv_state);
        
        for (const auto& res : results) {
            if (res.error) {
                std::cerr << "Protocol error from client " << client_fd << std::endl;
                cleanup_client(client_fd, epoll_fd, "protocol error");
                return;
            }
            
            std::string full_msg = "Client " + std::to_string(client_fd) + " says: " + res.message;
            std::cout << full_msg << std::endl;
            broadcast(client_fd, full_msg);
        }
    }
}

void ChatServer::handle_stdin() {
    char buf[1024];
    std::string input;
    while (true) {
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n > 0) {
            input.append(buf, n);
        } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            break;
        } else {
            break;
        }
    }
    
    if (!input.empty()) {
        std::string msg = "Server: " + input;
        std::cout << msg << std::endl;
        broadcast(-1, msg); // -1 表示不排除任何人，发给所有客户端
    }
}

void ChatServer::broadcast(int exclude_fd, const std::string& msg) {
    for (const auto& [fd, _] : clients_) {
        if (fd != exclude_fd) {
            send_message(fd, msg);
        }
    }
}

void ChatServer::check_timeouts() {
    auto now = std::chrono::steady_clock::now();
    std::vector<int> to_remove;

    for (const auto& [fd, info] : clients_) {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - info.last_active).count();
        if (duration > HEARTBEAT_TIMEOUT) {
            to_remove.push_back(fd);
        }
    }

    for (int fd : to_remove) {
        std::cout << "Client " << fd << " timed out." << std::endl;
        cleanup_client(fd, epoll_fd_, "timeout");
    }
}

void ChatServer::cleanup_client(int fd, int epoll_fd, const std::string& reason) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    
    std::string notice = "Client " + std::to_string(fd) + " left (" + reason + ")";
    broadcast(fd, notice); // 通知其他人
    
    clients_.erase(fd);
    std::cout << notice << std::endl;
}