#include "server.h"
#include <iostream>
#include <exception>

int main() {
    try {
        ChatServer server(8080);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}