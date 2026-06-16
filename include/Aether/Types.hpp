#pragma once

#include <functional>
#include <memory>
#include <string>
#include <cstdint>

namespace Aether {

// 连接标识符
using ConnectionID = uint64_t;

// 连接指针类型（前向声明）
class Connection;
using ConnectionPtr = std::shared_ptr<Connection>;
using ConnectionWeakPtr = std::weak_ptr<Connection>;

// 应用层回调类型
using AppLayerCallback = std::function<void(ConnectionPtr)>;
using HighWaterMarkCallback = std::function<void(ConnectionPtr, size_t)>;
using WriteCompleteCallback = std::function<void(ConnectionPtr)>;
using ServerCallback = std::function<void(ConnectionID, class Reactor*)>;

// FrameDecoder 工厂类型
class FrameDecoder;
using FrameDecoderFactory = std::function<std::unique_ptr<FrameDecoder>()>;

// ConnectionState 定义在 Connection.hpp 中

} // namespace Aether