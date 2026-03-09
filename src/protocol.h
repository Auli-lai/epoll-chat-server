#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <cstdint>
#include <vector>

constexpr int MAX_MESSAGE_LEN = 65536;

struct ParseResult {
    bool success;
    std::string message;
    bool error;
};

struct ClientRecvState {
    std::string buffer;
    int expected_len = -1; // -1 表示等待长度头
};

std::vector<ParseResult> parse_messages(ClientRecvState& state);

#endif // PROTOCOL_H