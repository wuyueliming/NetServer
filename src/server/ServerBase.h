#pragma once

#include "../common/Connection.h"
#include "../common/TimeWheel.h"

namespace NetWork {

// ServerBase 抽象接口类
// 定义服务器的统一接口，支持 TCP 实现
class ServerBase {
public:
    virtual ~ServerBase() = default;

    // 启动服务器
    virtual void start() = 0;
    // 停止服务器
    virtual void stop() = 0;

    //设置额外IO线程数
    virtual void setExtraThread(int count) = 0;

    // 设置应用层回调函数
    virtual void setMessageCallback(const AppLayerCallback& cb) = 0;
    virtual void setConnectionCallback(const AppLayerCallback& cb) = 0;
    virtual void setCloseCallback(const AppLayerCallback& cb) = 0;
    virtual void setEventCallback(const AppLayerCallback& cb) = 0;

    // 设置超时销毁
    virtual void enableInactiveRelease(uint32_t timeout) = 0;
    virtual void disableInactiveRelease() = 0;
};

} // namespace NetWork
