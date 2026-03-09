#include "protocol.h"
#include <cstring>
#include <arpa/inet.h>
#include <iostream>

std::vector<ParseResult> parse_messages(ClientRecvState& state) {
    std::vector<ParseResult> results;

    while (true) {
        // 1. 如果还没读到长度头
        if (state.expected_len == -1) {
            if (state.buffer.size() < 4) {
                break; // 数据不足，等待下次
            }
            
            uint32_t net_len;
            memcpy(&net_len, state.buffer.data(), 4);
            uint32_t msg_len = ntohl(net_len);

            if (msg_len > MAX_MESSAGE_LEN) {
                ParseResult err;
                err.success = false;
                err.error = true;
                err.message = "Message too large";
                results.push_back(err);
                return results; // 直接返回错误，上层会关闭连接
            }

            state.expected_len = static_cast<int>(msg_len);
            state.buffer.erase(0, 4); // 移除长度头
        }

        // 2. 检查是否有完整的数据包
        if (static_cast<size_t>(state.expected_len) <= state.buffer.size()) {
            ParseResult res;
            res.success = true;
            res.error = false;
            res.message = state.buffer.substr(0, state.expected_len);
            results.push_back(res);

            // 移除已处理的数据
            state.buffer.erase(0, state.expected_len);
            state.expected_len = -1; // 重置状态，准备解析下一个包
        } else {
            break; // 数据不够，等待下次
        }
    }
    return results;
}   