#pragma once

// Aether 网络库 - 统一入口头文件
// 用户只需 include 此文件即可使用所有核心功能

// 版本信息
#define AETHER_VERSION "1.0.0"
#define AETHER_VERSION_MAJOR 1
#define AETHER_VERSION_MINOR 0
#define AETHER_VERSION_PATCH 0

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
#include "Reactor.hpp"

// 协议组件
#include "FrameDecoder.hpp"

// 日志（可选）
#include "Log.hpp"

namespace Aether {
    // 库版本信息
    inline constexpr const char* Version() { return AETHER_VERSION; }
    inline constexpr int VersionMajor() { return AETHER_VERSION_MAJOR; }
    inline constexpr int VersionMinor() { return AETHER_VERSION_MINOR; }
    inline constexpr int VersionPatch() { return AETHER_VERSION_PATCH; }
}