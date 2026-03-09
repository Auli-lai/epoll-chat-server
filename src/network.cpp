#include "network.h"
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>

bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

int create_listener_socket(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    if (listen(sock, SOMAXCONN) < 0) {
        close(sock);
        return -1;
    }

    set_nonblocking(sock); // 监听套接字也设为非阻塞
    return sock;
}

bool send_message(int fd, const std::string& msg) {
    uint32_t len = htonl(msg.size());
    if (write(fd, &len, 4) != 4) return false;
    if (write(fd, msg.data(), msg.size()) != (ssize_t)msg.size()) return false;
    return true;
}