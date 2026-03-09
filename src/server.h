#ifndef SERVER_H
#define SERVER_H

#include <unordered_map>
#include <chrono>
#include <vector>
#include <sys/epoll.h>
#include "protocol.h"

typedef int sig_atomic_t;

struct ClientInfo {
    ClientRecvState recv_state;
    std::chrono::steady_clock::time_point last_active;
};

class ChatServer {
public:
    explicit ChatServer(int port);
    ~ChatServer();

    // 启动服务器主循环
    void run();
    // 停止服务器
    void stop();

private:
    void handle_accept(int server_fd, int epoll_fd);
    void handle_client_data(int client_fd, int epoll_fd);
    void handle_stdin(int epoll_fd);
    void check_timeouts();
    void cleanup_client(int fd, int epoll_fd, const std::string& reason = "");
    void broadcast(int exclude_fd, const std::string& msg);

    int port_;
    int server_fd_;
    int epoll_fd_;
    volatile sig_atomic_t running_;
    
    std::unordered_map<int, ClientInfo> clients_;
    static constexpr int HEARTBEAT_TIMEOUT = 30; // 秒
};

#endif // SERVER_H