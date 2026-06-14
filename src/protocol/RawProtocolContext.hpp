#pragma once

#include "../server/ProtocolContext.hpp"
#include <string>
#include <queue>

namespace Aether {

// RawProtocolContext: 最简单的协议上下文，将每次读取的数据直接作为完整报文
// 适用于回显服务器等不需要协议解析的场景
class RawProtocolContext : public ProtocolContext {
public:
    void Parse(BufferReader &reader) override {
        // 将缓冲区中所有可读数据作为一个报文
        while (reader.ReadAbleSize() > 0) {
            std::string msg = reader.ReadAsStringAndPop(reader.ReadAbleSize());
            _messages.push(std::move(msg));
        }
    }

    bool HasMessage() const override {
        return !_messages.empty();
    }

    std::any PopMessage() override {
        std::string msg = std::move(_messages.front());
        _messages.pop();
        return msg;
    }

    int ErrorStatus() const override {
        return 0;
    }

    std::any GetPartialMessage() const override {
        return std::string();
    }

    void Reset() override {
        while (!_messages.empty()) {
            _messages.pop();
        }
    }

private:
    std::queue<std::string> _messages;
};

} // namespace Aether
