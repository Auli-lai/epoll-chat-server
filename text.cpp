//ET
#include <iostream>
#include <string>
#include <signal.h>
#include <vector>
#include <sys/socket.h>
#include <unordered_map>
#include <chrono>
#include <arpa/inet.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/epoll.h>
#include <cerrno>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <cstdint>
#include <algorithm>
#include <condition_variable>

using std::cout;
using std::cerr;
using std::flush;
using std::endl;

constexpr int port = 8080;
constexpr int max_message_len = 65536;
constexpr int heartbeat_interval = 15; // 秒
constexpr int heartbeat_timeout = heartbeat_interval * 2; // 超时阈值

volatile sig_atomic_t running = 1;

void handle_sigint(int) { running = 0; }

struct client_state{

    std::string recv_buffer;
    int expected_len = -1;
    std::chrono::steady_clock::time_point last_active;

};


bool set_nonblocking(int fd){

    int now_flag = fcntl(fd,F_GETFL,0);
    if(now_flag == -1) return false;
    return fcntl(fd,F_SETFL,now_flag | O_NONBLOCK) != -1;

}

void send_message(int fd,const std::string& msg){

    if(msg.empty()) return;

    uint32_t len = htonl(static_cast<uint32_t> (msg.size()));
    std::string packet;
    packet.append(reinterpret_cast<char*>(&len),4);
    packet.append(msg);
    write(fd,packet.data(),packet.size());

}

int main(){

    signal(SIGPIPE,SIG_IGN);//sigpipe,sig_ign
    signal(SIGINT, handle_sigint);//sigint

    int server_fd = socket(AF_INET,SOCK_STREAM,0);
    if(server_fd < 0){

        cerr << "socket : " << strerror(errno) << endl;
        return -1;

    }

    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if(bind(server_fd,(sockaddr*)&server_addr,sizeof(server_addr)) < 0){
    
        cerr << "bind : " << strerror(errno) << endl;
        close(server_fd);
        return -1;

    }

    if(listen(server_fd,10) < 0){

        cerr << "listen : " << strerror(errno) << endl;
        close(server_fd);
        return -1;

    }

    if(!set_nonblocking(server_fd)){

        cerr << "false of set_nonblocking : " << strerror(errno) << endl;
        close(server_fd);
        return -1;
    
    }

    cout << "server set already " << endl;
    

    int epoll_fd =  epoll_create1(0);
    if(epoll_fd < 0){

        cerr << "epoll_creat1 : " << strerror(errno) << endl;
        close(server_fd);
        return -1;

    }

    epoll_event server_event;
    server_event.data.fd = server_fd;
    server_event.events = EPOLLIN | EPOLLET;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &server_event);

    if(!set_nonblocking(STDIN_FILENO)){

        cerr << "false of set_nonblocking : " << strerror(errno) << endl;
        return -1;
    
    }

    epoll_event stdin_event;
    stdin_event.data.fd = STDIN_FILENO;
    stdin_event.events = EPOLLIN | EPOLLET;
    
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,STDIN_FILENO,&stdin_event);

    std::vector<epoll_event> events(100);
    std::unordered_map<int,client_state> client_states; 
    while(running){

        int nfds = epoll_wait(epoll_fd,events.data(),events.size(),1000);
        if (nfds < 0) {
            if (errno == EINTR) continue; // 被信号中断，继续循环（此时 running 可能已为 0）
            cerr << "epoll_wait error: " << strerror(errno) << endl;
            break;
        }
    
        for(int i = 0;i < nfds; ++i){

            int now_fd = events[i].data.fd;

            if(now_fd == server_fd){

                while(true){
                    
                    sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    
                    int client_fd = accept(now_fd,(sockaddr*)&client_addr,&client_len);
                    
                    if(client_fd < 0){

                        if(errno == EAGAIN || errno == EWOULDBLOCK){

                            break;

                        }else{

                            cerr << "accept : " << strerror(errno) << endl;
                            break;

                        }

                    }

                    if(!set_nonblocking(client_fd)){

                        close(client_fd);
                        continue;

                    }

                    client_states.emplace(client_fd,client_state{});//记录client_fd状态
                    client_states[client_fd].last_active = std::chrono::steady_clock::now();
                    
                    epoll_event client_event;
                    client_event.data.fd = client_fd;
                    client_event.events = EPOLLIN | EPOLLET;
                    
                    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_fd,&client_event);
                    
                    char ip_str[INET_ADDRSTRLEN];

                    if(inet_ntop(AF_INET,&client_addr.sin_addr,ip_str,sizeof(ip_str)) == nullptr){

                        cerr << "inet_ntop : " << strerror(errno) << endl;
                        continue;

                    }

                    cout << "client_message : " << "\n" 
                         << "client_fd : " << client_fd << "\n"
                         << "client_ip : " << ip_str << "\n"
                         << "client_port : " << ntohs(client_addr.sin_port)
                         << endl;


                }

            }else if(now_fd == STDIN_FILENO){

                std::string all_message = "";
                char buffer[1024];
                
                while(true){

                    int read_len = read(now_fd,buffer,sizeof(buffer));
                    
                    if(read_len > 0){
                        
                        all_message.append(buffer,read_len);

                    }else if(read_len == 0){

                        cout << "stdin want break" << endl;
                        break;

                    }else{

                        if(errno == EAGAIN || errno == EWOULDBLOCK){

                            break;

                        }else{

                            cerr << "read : " << strerror(errno) << endl;
                            break;

                        }

                    }

                }

                cout << "stdin say : " << all_message << endl; 
                
            }else{

                std::string all_message = "";
                
                char buffer[1024];
                
                while(true){

                    int read_len = read(now_fd,buffer,sizeof(buffer));

                    if(read_len > 0){

                        bool close_due_to_error = false;
                        
                        client_states[now_fd].recv_buffer.append(buffer,read_len);

                        while(true){

                            client_state& state = client_states[now_fd];
                            
                            if(state.expected_len == -1){

                                if(state.recv_buffer.size() < 4) break;
                                uint32_t net_len;
                                memcpy(&net_len,state.recv_buffer.data(),4);
                                uint32_t msg_len = ntohl(net_len);
                                if(msg_len > max_message_len){

                                    cerr << "too long long !!!" << endl;
                                    close_due_to_error = true;
                                    break;

                                }
                                state.expected_len = static_cast<int>(msg_len);
                                state.recv_buffer.erase(0,4);

                            }

                            if(static_cast<size_t>(state.expected_len) <= state.recv_buffer.size()){

                                client_states[now_fd].last_active = std::chrono::steady_clock::now();
                                std::string message = state.recv_buffer.substr(0,state.expected_len);
                                all_message.append(message);
                                state.recv_buffer.erase(0,state.expected_len);
                                state.expected_len = -1;

                            }else{
                                break;
                            }

                        }
                        if (close_due_to_error) {
                            // 关闭连接并清理
                            close(now_fd);
                            client_states.erase(now_fd);
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, now_fd, nullptr);
                            continue; // 跳过后续的广播逻辑
                        }

                    }else if(read_len == 0){

                        for(const auto& [fd,state] : client_states){

                            if(fd != now_fd){

                                std::string temp_message = "client_fd : " + std::to_string(now_fd) + "disconnected\n";
                                send_message(fd,temp_message);
                            }
                        }

                        cout << "client_fd : " <<  now_fd << "disconnected" << endl;

                        close(now_fd);
                        client_states.erase(now_fd);
                        epoll_ctl(epoll_fd,EPOLL_CTL_DEL,now_fd,nullptr);
                        break;

                    }else{

                        if(errno == EAGAIN || errno == EWOULDBLOCK){
                            
                            break;
                        
                        }else{

                            cerr << "read : " << strerror(errno) << endl;
                            
                            close(now_fd);
                            client_states.erase(now_fd);
                            epoll_ctl(epoll_fd,EPOLL_CTL_DEL,now_fd,nullptr);
                            break;

                        }


                    }

                }

            
                if (!all_message.empty()) {
                
                    cout << "client_fd : " << now_fd << " say : " << all_message << endl;

                    for (const auto& [fd,state] : client_states) {
                        if (fd != now_fd) {
                            std::string temp_message = "client_fd : " + std::to_string(now_fd) + " say : " + all_message;
                            send_message(fd,temp_message);
                        }
                    }
                }


            }

        }

        auto now = std::chrono::steady_clock::now();
        std::vector<int> to_close;

        for (const auto& [fd, state] : client_states) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                now - state.last_active
            ).count();
            if (duration > heartbeat_timeout) {
                to_close.push_back(fd);
            }
        }

        // 关闭超时客户端
        for (int fd : to_close) {
            cout << "Client " << fd << " timed out (no heartbeat)." << endl;
            
            // 通知其他客户端
            std::string notice = "Client " + std::to_string(fd) + " disconnected (timeout).";
            for (const auto& [other_fd, _] : client_states) {
                if (other_fd != fd) {
                    send_message(other_fd, notice);
                }
            }

            close(fd);
            client_states.erase(fd);
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
        }

    }
    cout << "Shutting down... closing all client connections." << endl;
    for (const auto& [fd, _] : client_states) {
        close(fd);
    }
    client_states.clear();
    close(epoll_fd);    
    close(server_fd);
    cout << "server exited" << endl;
    return 0;
}