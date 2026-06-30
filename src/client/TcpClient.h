#pragma once

#include "../common/EventLoop.h"
#include "../common/InetAddr.hpp"
#include "../common/noncopyable.hpp"
#include "Connector.h"
#include "../common/TcpConnection.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <atomic>
#include <thread>

namespace NetWork {

// TcpClient：异步 TCP 客户端
// 线程安全：connect/disconnect/stop 可在任意线程调用
// 使用方式：用户自行创建 EventLoop 并初始化，connect() 仅发起连接，用户需自行调用 loop.loop()
class TcpClient : public noncopyable {
public:
    using ConnectionCallback = AppLayerCallback;
    using MessageCallback = AppLayerCallback;

    // 构造函数：用户自行创建 EventLoop 并初始化进 client
    TcpClient(EventLoop* loop, const InetAddr& serverAddr, const std::string& name);
    ~TcpClient();

    // 发起连接（线程安全），仅发起连接，不创建线程运行 loop()
    void connect();
    // 断开连接（线程安全）
    void disconnect();
    // 停止连接尝试和 EventLoop (幂等, 线程安全)
    void stop();

    // 获取当前连接（线程安全）
    std::shared_ptr<TcpConnection> connection() const;
    EventLoop* getLoop() const { return _loop; }

    // 是否启用自动重连
    bool retry() const { return _retry; }
    void enableRetry()  { _retry = true; }
    void disableRetry() { _retry = false; }
    const std::string& name() const { return _name; }

    // 设置回调
    void setConnectionCallback(ConnectionCallback cb) { _connectionCallback = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { _messageCallback = std::move(cb); }
    void setWriteCompleteCallback(const AppLayerCallback& cb) { _writeCompleteCallback = cb; }

private:
    // EventLoop 线程中执行
    void newConnection(int sockfd);
    void removeConnection(const std::shared_ptr<TcpConnection>& conn);

    EventLoop* _loop;                                    // 外部传入的 EventLoop，不拥有所有权
    std::shared_ptr<Connector> _connector;
    std::string _name;

    // 用户回调
    ConnectionCallback _connectionCallback;
    MessageCallback _messageCallback;
    AppLayerCallback _writeCompleteCallback;

    std::atomic<bool> _retry{false};    // 自动重连
    std::atomic<bool> _connect{false};  // 用户期望连接（原子变量，跨线程安全）
    std::atomic<bool> _started{false};  // 是否已启动
    uint64_t _nextConnId{1};

    mutable std::mutex _mutex;
    std::shared_ptr<TcpConnection> _connection; // GUARDED_BY(_mutex)
};

} // namespace NetWork
