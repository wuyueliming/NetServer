#pragma once

#include "Connection.h"
#include "TimeWheel.h"

namespace Aether {

// ServerBase 抽象接口类
// 定义服务器的统一接口，支持 TCP 实现
class ServerBase {
public:
    virtual ~ServerBase() = default;

    // 启动服务器
    virtual void start() = 0;
    // 停止服务器
    virtual void stop() = 0;

    // 设置线程池大小（从 Reactor 数量）
    virtual void SetThreadCount(int count) = 0;

    // 设置应用层回调函数
    virtual void SetMessageCallback(const AppLayerCallback& cb) = 0;
    virtual void SetConnectedCallback(const AppLayerCallback& cb) = 0;
    virtual void SetClosedCallback(const AppLayerCallback& cb) = 0;
    virtual void SetEventCallback(const AppLayerCallback& cb) = 0;

    // 设置超时销毁
    virtual void EnableInactiveRelease(uint32_t timeout) = 0;
    virtual void DisableInactiveRelease() = 0;

    // 设置帧解码器工厂（每个连接创建独立实例）
    virtual void SetFrameDecoderFactory(FrameDecoderFactory factory) = 0;
};

} // namespace Aether
