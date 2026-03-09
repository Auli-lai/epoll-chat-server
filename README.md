# Epoll Chat Server

A simple, high-performance TCP chat server using Linux `epoll` in **Edge-Triggered (ET)** mode.  
Designed for learning network programming and I/O multiplexing — clean, minimal, and beginner-friendly.

## ✨ Features

- **Length-prefixed binary protocol** to handle TCP packet sticking and splitting
- **High concurrency**: Supports 1000+ simultaneous connections (single-threaded)
- **Epoll in Edge-Triggered (ET) mode** for efficient event handling
- **Zero third-party dependencies** — pure C++17 and standard Linux syscalls
- **Realistic chat simulation**: Broadcast messaging + idle timeout (30s heartbeat)

## 🚀 Quick Start

### Prerequisites
- Linux or macOS (Windows requires WSL)
- CMake (≥ 3.10)
- g++ (≥ 7.0) or clang++ with C++17 support

### Build & Run

```bash
# 1. Clone the repository
git clone https://github.com/Auli-lai/epoll-chat-server.git
cd epoll-chat-server

# 2. Create a build directory and configure with CMake
mkdir build && cd build
cmake ..

# 3. Compile the server
make

# 4. Run the server (default port 8080)
./epoll_chat_server
