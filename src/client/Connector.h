#pragma once

#include "Reactor.h"
#include "base/InetAddr.hpp"
#include "base/noncopyable.hpp"
#include "Channel.h"

#include <functional>
#include <memory>
#include <atomic>

namespace Aether {

// Connector：异步发起非阻塞 TCP 连接，支持指数退避重连
class Connector : public noncopyable,
                  public std::enable_shared_from_this<Connector> {
public:
    using NewConnectionCallback = std::function<void(int sockfd)>;

    Connector(Reactor* loop, const InetAddr& serverAddr);
    ~Connector();

    void setNewConnectionCallback(const NewConnectionCallback& cb) { _newConnectionCallback = cb; }

    // 线程安全：可在任意线程调用
    void start();
    // 必须在 Reactor 线程中调用（由 TcpClient::removeConnection 使用）
    void restart();
    // 线程安全：可在任意线程调用
    void stop();

    const InetAddr& serverAddress() const { return _serverAddr; }

private:
    enum State { kDisconnected, kConnecting, kConnected };
    static const int kInitRetryDelayMs = 1000;
    static const int kMaxRetryDelayMs  = 30 * 1000;

    void setState(State s) { _state.store(s, std::memory_order_relaxed); }

    void startInLoop();
    void stopInLoop();
    void connect();
    void connecting(int sockfd);
    void handleWrite();
    void handleError();
    void retry(int sockfd);
    int removeAndResetChannel();
    void resetChannel();

    Reactor* _loop;
    InetAddr _serverAddr;
    std::atomic<bool> _connect;    // 是否期望连接（原子变量，跨线程安全）
    std::atomic<State> _state;     // 当前连接状态（原子变量，跨线程安全）
    std::unique_ptr<Channel> _channel;
    NewConnectionCallback _newConnectionCallback;
    int _retryDelayMs;      // 当前重试延迟（指数退避）
};

} // namespace Aether
