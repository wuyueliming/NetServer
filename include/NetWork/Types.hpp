#pragma once

#include <functional>
#include <memory>
#include <string>
#include <cstdint>

namespace NetWork {

// 连接标识符
using ConnectionID = uint64_t;

// 连接指针类型（前向声明）
class Connection;
using ConnectionPtr = std::shared_ptr<Connection>;
using ConnectionWeakPtr = std::weak_ptr<Connection>;

// 应用层回调类型
using AppLayerCallback = std::function<void(ConnectionPtr)>;
using WriteCompleteCallback = std::function<void(ConnectionPtr)>;
using ServerCallback = std::function<void(ConnectionID, class EventLoop*)>;

// ConnectionState 定义在 Connection.hpp 中

} // namespace NetWork