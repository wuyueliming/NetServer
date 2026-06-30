#pragma once

// NetWork 网络库 - 统一入口头文件
// 用户只需 include 此文件即可使用所有核心功能

// 版本信息
#define NETWORK_VERSION "1.0.0"
#define NETWORK_VERSION_MAJOR 1
#define NETWORK_VERSION_MINOR 0
#define NETWORK_VERSION_PATCH 0

// 核心类型定义
#include "Types.hpp"

// IO 基础组件
#include "Buffer.hpp"
#include "InetAddr.hpp"

// 网络组件
#include "Connection.hpp"
#include "TcpServer.hpp"
#include "TcpClient.hpp"
#include "ServerBase.hpp"
#include "EventLoop.hpp"

// 日志（可选）
#include "Log.hpp"

namespace NetWork {
    // 库版本信息
    inline constexpr const char* Version() { return NETWORK_VERSION; }
    inline constexpr int VersionMajor() { return NETWORK_VERSION_MAJOR; }
    inline constexpr int VersionMinor() { return NETWORK_VERSION_MINOR; }
    inline constexpr int VersionPatch() { return NETWORK_VERSION_PATCH; }
}