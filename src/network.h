#ifndef NETWORK_H
#define NETWORK_H

#include <sys/socket.h>
#include <string>

// 设置文件描述符为非阻塞模式
bool set_nonblocking(int fd);

// 创建并配置监听套接字
int create_listener_socket(int port);

// 发送消息（包含长度前缀）
bool send_message(int fd, const std::string& msg);

#endif // NETWORK_H