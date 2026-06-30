#include "TcpClient.h"

#include "Logger.hpp"

#include <cassert>
#include <cstdio>

namespace NetWork {

TcpClient::TcpClient(EventLoop* loop, const InetAddr& serverAddr, const std::string& name)
    : _loop(loop),
      _connector(std::make_shared<Connector>(loop, serverAddr)),
      _name(name)
{
    _connector->setNewConnectionCallback(
        [this](int sockfd) { newConnection(sockfd); });
    LOG(INFO) << "TcpClient[" << _name << "] created, server=" << serverAddr.StrAddr();
}

TcpClient::~TcpClient() {
    LOG(INFO) << "TcpClient[" << _name << "] destructing";
    stop();
}

void TcpClient::connect() {
    if (_started.load()) return;
    _started.store(true);
    _connect.store(true, std::memory_order_relaxed);

    // 发起连接，不创建线程运行 loop()，由用户自行调用 _loop->loop()
    _connector->start();
}

void TcpClient::disconnect() {
    _connect.store(false, std::memory_order_relaxed);
    std::shared_ptr<TcpConnection> conn;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        conn = _connection;
    }
    if (conn) {
        conn->Shutdown();
    }
}

void TcpClient::stop() {
    _connect.store(false, std::memory_order_relaxed);
    if (!_started.load()) return;

    // 1. 停止 Connector
    _connector->stop();

    // 2. 断开连接
    std::shared_ptr<TcpConnection> conn;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        conn = _connection;
    }
    if (conn) {
        conn->Release();
    }

    // 3. 退出 EventLoop
    _loop->Quit();

    _started.store(false);
}

std::shared_ptr<TcpConnection> TcpClient::connection() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _connection;
}

void TcpClient::newConnection(int sockfd) {
    assert(_loop->isInLoopThread());

    // 获取对端地址
    struct sockaddr_in peerAddrRaw;
    socklen_t addrlen = sizeof(peerAddrRaw);
    ::getpeername(sockfd, (struct sockaddr*)&peerAddrRaw, &addrlen);
    InetAddr peerAddr(peerAddrRaw);

    ConnectionID connID = _nextConnId++;

    // 创建 TcpConnection（复用服务端连接类）
    auto conn = std::make_shared<TcpConnection>(_loop, connID, sockfd, peerAddr);

    conn->SetConnectedCallback(_connectionCallback);
    conn->SetMessageCallback(_messageCallback);
    conn->SetWriteCompleteCallback(_writeCompleteCallback);
    // 客户端不需要 setReleaseCallback（没有映射表）
    // 设置连接断开时重新连接
    conn->SetClosedCallback([this](const ConnectionPtr& c) {
        auto tcpConn = std::static_pointer_cast<TcpConnection>(c);
        removeConnection(tcpConn);
    });

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _connection = conn;
    }

    // 建立连接，开始事件监听
    conn->Established();
}

void TcpClient::removeConnection(const std::shared_ptr<TcpConnection>& conn) {
    assert(_loop->isInLoopThread());

    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_connection == conn) {
            _connection.reset();
        }
    }

    // 通知上层连接已断开 (conn 此时已 DISCONNECTED, isConnected()==false)
    if (_connectionCallback) {
        _connectionCallback(conn);
    }

    // 自动重连
    if (_retry.load(std::memory_order_relaxed) && _connect.load(std::memory_order_relaxed)) {
        LOG(INFO) << "TcpClient[" << _name << "] reconnecting to "
                  << _connector->serverAddress().StrAddr();
        _connector->restart();
    }
}

} // namespace NetWork
