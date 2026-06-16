#pragma once

#include "Reactor.h"
#include "base/InetAddr.hpp"
#include "base/noncopyable.hpp"
#include "Connector.h"
#include "TcpConnection.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace Aether {

// TcpClient：异步 TCP 客户端
// 参考 muduo::TcpClient 设计
// 线程安全：connect/disconnect/stop 可在任意线程调用
class TcpClient : public noncopyable {
public:
    using ConnectionCallback = AppLayerCallback;
    using MessageCallback = AppLayerCallback;

    TcpClient(Reactor* loop, const InetAddr& serverAddr, const std::string& name);
    ~TcpClient();

    // 发起连接（线程安全）
    void connect();
    // 断开连接（线程安全）
    void disconnect();
    // 停止连接尝试（线程安全）
    void stop();

    // 获取当前连接（线程安全）
    std::shared_ptr<TcpConnection> connection() const;
    Reactor* getLoop() const { return _loop; }

    // 是否启用自动重连
    bool retry() const { return _retry; }
    void enableRetry() { _retry = true; }
    const std::string& name() const { return _name; }

    // 设置回调
    void setConnectionCallback(ConnectionCallback cb) { _connectionCallback = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { _messageCallback = std::move(cb); }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { _writeCompleteCallback = cb; }

private:
    // Reactor 线程中执行
    void newConnection(int sockfd);
    void removeConnection(const std::shared_ptr<TcpConnection>& conn);

    Reactor* _loop;
    std::shared_ptr<Connector> _connector;
    std::string _name;

    // 用户回调
    ConnectionCallback _connectionCallback;
    MessageCallback _messageCallback;
    WriteCompleteCallback _writeCompleteCallback;

    std::atomic<bool> _retry;    // 自动重连
    std::atomic<bool> _connect;  // 用户期望连接（原子变量，跨线程安全）
    uint64_t _nextConnId;

    mutable std::mutex _mutex;
    std::shared_ptr<TcpConnection> _connection; // GUARDED_BY(_mutex)
};

} // namespace Aether
