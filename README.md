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
- g++ (≥7.0) or clang++ with C++17 support

### Option 1: Quick Copy-Paste (for testing)
Simply copy the code from [`text.cpp`](text.cpp) and compile locally.


Method 2:
Run the following instructions:
### Build & Run
```bash
# Clone the repo
git clone https://github.com/Auli-lai/epoll-chat-server.git
cd epoll-chat-server

# Compile
g++ -std=c++17 -O2 -o server server.cpp

# Run
./server
```
# Configuration

-The default port for this epoll server is 8080
-Default communication uses the IPv4 protocol
-Default listen to all local interfaces


## 📄 License
This project is licensed under the [MIT License](LICENSE).
