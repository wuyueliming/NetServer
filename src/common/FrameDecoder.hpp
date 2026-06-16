#pragma once

#include <string>
#include <cstdint>
#include <algorithm>
#include "base/Buffer.hpp"

namespace Aether {

// FrameDecoder：从字节流中切分出完整报文（粘包处理）
// 职责单一：只做 Framing，不解析语义
class FrameDecoder {
public:
    virtual ~FrameDecoder() = default;

    // 从 buffer 中尝试取出一个完整报文
    // true  → out_frame 包含完整报文，buffer 已消费相应字节
    // false → 数据还不够一个完整报文，buffer 未消费
    virtual bool TryDecode(Buffer &buffer, std::string &out_frame) = 0;

    // 重置帧解码器状态（连接关闭或上下文重置时调用）
    virtual void Reset() {}
};

// ====== 内置实现 ======

// NoopFrameDecoder：每次将全部可读数据作为一个报文
class NoopFrameDecoder : public FrameDecoder {
public:
    bool TryDecode(Buffer &buffer, std::string &out_frame) override {
        if (buffer.ReadAbleSize() == 0) return false;
        out_frame = buffer.ReadAsStringAndPop(buffer.ReadAbleSize());
        return true;
    }
};

// LineFrameDecoder：按 \n 分隔报文（自动处理 \r\n 和 \n）
class LineFrameDecoder : public FrameDecoder {
public:
    explicit LineFrameDecoder(size_t max_frame_size = 8192)
        : _max_frame_size(max_frame_size) {}

    bool TryDecode(Buffer &buffer, std::string &out_frame) override {
        char *lf = buffer.FindLF();
        if (lf == nullptr) return false;  // 还没有完整的一行

        // 计算从当前位置到 \n 的长度（包含 \n）
        size_t len = lf - buffer.ReadPosition() + 1;

        if (len > _max_frame_size) {
            // 行太长，跳过该行
            buffer.MoveReadOffset(len);
            return false;
        }

        // 读取完整行（包含 \n）
        out_frame = buffer.ReadAsStringAndPop(len);

        // 剥离行尾分隔符（\r\n 或 \n），方便 Codec 直接使用
        if (out_frame.size() >= 2 && out_frame[out_frame.size() - 2] == '\r') {
            out_frame.resize(out_frame.size() - 2);  // 去掉 \r\n
        } else {
            out_frame.pop_back();  // 去掉 \n
        }

        return true;
    }

    void Reset() override {
        // 无状态，无需重置
    }

private:
    size_t _max_frame_size;
};

// LengthFieldFrameDecoder：[4字节大端长度前缀] + [payload]
class LengthFieldFrameDecoder : public FrameDecoder {
public:
    explicit LengthFieldFrameDecoder(size_t max_frame_size = 64 * 1024 * 1024)
        : _max_frame_size(max_frame_size) {}

    bool TryDecode(Buffer &buffer, std::string &out_frame) override {
        while (buffer.ReadAbleSize() > 0) {
            switch (_state) {
            case NEED_LENGTH:
                if (buffer.ReadAbleSize() < 4) return false;

                {
                    const char *pos = buffer.ReadPosition();
                    _expected_length = (static_cast<uint32_t>(static_cast<uint8_t>(pos[0])) << 24) |
                                       (static_cast<uint32_t>(static_cast<uint8_t>(pos[1])) << 16) |
                                       (static_cast<uint32_t>(static_cast<uint8_t>(pos[2])) << 8)  |
                                       (static_cast<uint32_t>(static_cast<uint8_t>(pos[3])));
                    buffer.MoveReadOffset(4);

                    if (_expected_length > _max_frame_size) {
                        // 帧长度超过限制，进入 SKIPPING 状态跳过 payload
                        _skip_remaining = _expected_length;
                        _state = SKIPPING;
                        continue;
                    }

                    _state = NEED_PAYLOAD;

                    if (_expected_length == 0) {
                        _state = NEED_LENGTH;
                        out_frame.clear();
                        return true;
                    }
                }
                [[fallthrough]];

            case NEED_PAYLOAD:
                if (buffer.ReadAbleSize() < _expected_length) return false;
                out_frame = buffer.ReadAsStringAndPop(_expected_length);
                _state = NEED_LENGTH;
                return true;

            case SKIPPING:
                {
                    size_t can_skip = std::min(_skip_remaining, buffer.ReadAbleSize());
                    buffer.MoveReadOffset(can_skip);
                    _skip_remaining -= can_skip;
                    if (_skip_remaining == 0) {
                        _state = NEED_LENGTH;
                    }
                    return false;  // 跳过期间不产出帧
                }
            default:
                return false;
            }
        }
        return false;
    }

    void Reset() override { _state = NEED_LENGTH; _expected_length = 0; _skip_remaining = 0; }

private:
    enum State { NEED_LENGTH, NEED_PAYLOAD, SKIPPING };
    State _state = NEED_LENGTH;
    uint32_t _expected_length = 0;
    size_t _skip_remaining = 0;
    size_t _max_frame_size;
};

} // namespace Aether