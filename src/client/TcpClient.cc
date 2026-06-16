#include "TcpClient.h"

#include "base/Logger.hpp"

#include <cassert>
#include <cstdio>

namespace Aether {

TcpClient::TcpClient(Reactor* loop, const InetAddr& serverAddr, const std::string& name)
    : _loop(loop),
      _connector(std::make_shared<Connector>(loop, serverAddr)),
      _name(name),
      _retry(false),
      _connect(false),
      _nextConnId(1)
{
    _connector->setNewConnectionCallback(
        [this](int sockfd) { newConnection(sockfd); });
    LOG(INFO) << "TcpClient[" << _name << "] created, server=" << serverAddr.StrAddr();
}

TcpClient::~TcpClient() {
    LOG(INFO) << "TcpClient[" << _name << "] destructing";
    // 析构时清理连接
    std::shared_ptr<TcpConnection> conn;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        conn = _connection;
    }

    if (conn) {
        // 连接存在，在 IO 线程中强制释放
        conn->Release();
    } else {
        // 无连接时，停止 Connector
        _connector->stop();
    }
}

void TcpClient::connect() {
    _connect.store(true, std::memory_order_relaxed);
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
    _connector->stop();
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

    // 自动重连
    if (_retry.load(std::memory_order_relaxed) && _connect.load(std::memory_order_relaxed)) {
        LOG(INFO) << "TcpClient[" << _name << "] reconnecting to "
                  << _connector->serverAddress().StrAddr();
        _connector->restart();
    }
}

} // namespace Aether
