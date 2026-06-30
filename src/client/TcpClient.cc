#include "TcpClient.h"

#include "Logger.hpp"

#include <cassert>
#include <netinet/in.h>
#include <sys/socket.h>

namespace NetWork {

TcpClient::TcpClient(EventLoop* loop, const InetAddr& serverAddr, const std::string& name)
    : _loop(loop),
      _name(name)
{
    _server_addrs.push_back(serverAddr);
    LOG(INFO) << "TcpClient[" << _name << "] created (single), server=" << serverAddr.StrAddr();
}

TcpClient::TcpClient(EventLoop* loop, const std::vector<InetAddr>& serverAddrs, const std::string& name)
    : _loop(loop),
      _name(name),
      _server_addrs(serverAddrs)
{
    LOG(INFO) << "TcpClient[" << _name << "] created (multi), hosts=" << serverAddrs.size();
}

TcpClient::~TcpClient() {
    LOG(INFO) << "TcpClient[" << _name << "] destructing";
    stop();
}

void TcpClient::setExtraThread(int count) {
    _loop_thread_pool.SetThreadCount(count < 0 ? 0 : count);
}

void TcpClient::connect() {
    if (_started.load()) return;
    _started.store(true);
    _connect.store(true, std::memory_order_relaxed);

    _loop_thread_pool.Create();

    //为每个地址创建Connector
    for (const auto& addr : _server_addrs) {
        auto connector = std::make_shared<Connector>(_loop, addr);
        connector->setNewConnectionCallback(
            [this, connector](int sockfd) { newConnection(sockfd, connector); });
        _connectors.push_back(connector);
        connector->start();
    }
}

void TcpClient::disconnect() {
    _connect.store(false, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto& [id, conn] : _connections) {
        if (conn) conn->Shutdown();
    }
}

void TcpClient::stop() {
    _connect.store(false, std::memory_order_relaxed);
    if (!_started.load()) return;
    _started.store(false);

    //1. 停止所有connector
    for (auto& connector : _connectors) {
        if (connector) connector->stop();
    }

    //2. 释放所有连接
    unordered_map<ConnectionID, std::shared_ptr<TcpConnection>> conns;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        conns.swap(_connections);
    }
    for (auto& [id, conn] : conns) {
        if (conn) conn->Release();
    }

    //3. 停止线程池
    _loop_thread_pool.Stop();

    //不退出_loop
}

void TcpClient::newConnection(int sockfd, std::shared_ptr<Connector> connector) {
    //在_loop线程
    assert(_loop->isInLoopThread());

    ConnectionID connID = _nextid.fetch_add(1);
    EventLoop* ioLoop = getNextLoop();

    struct sockaddr_in peerAddrRaw;
    socklen_t addrlen = sizeof(peerAddrRaw);
    ::getpeername(sockfd, (struct sockaddr*)&peerAddrRaw, &addrlen);
    InetAddr peerAddr(peerAddrRaw);

    auto conn = std::make_shared<TcpConnection>(ioLoop, connID, sockfd, peerAddr);

    conn->SetConnectedCallback(_connected_cb);
    conn->SetMessageCallback(_message_cb);
    conn->SetWriteCompleteCallback(_writeComplete_cb);
    //连接断开回调removeConnection
    conn->SetClosedCallback([this, connID, connector](const ConnectionPtr& c) {
        auto tcpConn = std::static_pointer_cast<TcpConnection>(c);
        removeConnection(connID, connector);
    });

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _connections[connID] = conn;
    }

    LOG(INFO) << "TcpClient[" << _name << "] new connection connID:" << connID
              << " peer:" << peerAddr.StrAddr();

    conn->Established();
}

void TcpClient::removeConnection(ConnectionID connID, std::shared_ptr<Connector> connector) {
    //在IO loop线程
    std::shared_ptr<TcpConnection> conn;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _connections.find(connID);
        if (it != _connections.end()) {
            conn = it->second;
            _connections.erase(it);
        }
    }

    //通知上层连接已断开
    if (conn && _connected_cb) {
        _connected_cb(conn);
    }

    //重连: restart必须在_loop线程
    if (_retry.load(std::memory_order_relaxed) && _connect.load(std::memory_order_relaxed)) {
        LOG(INFO) << "TcpClient[" << _name << "] reconnecting to "
                  << connector->serverAddress().StrAddr();
        _loop->runInLoop([connector]() { connector->restart(); });
    }
}

EventLoop* TcpClient::getNextLoop() {
    const auto& loops = _loop_thread_pool.GetAllLoops();
    if (loops.empty()) return _loop;  //extraThread=0时connector和IO都在_loop

    //extraThread>0时IO在池loop
    if (_load_balancer) {
        size_t idx = _load_balancer(loops);
        return loops[idx % loops.size()];
    }
    size_t idx = _next_loop_idx.fetch_add(1) % loops.size();
    return loops[idx];
}

std::shared_ptr<TcpConnection> TcpClient::GetConnection() const {
    std::lock_guard<std::mutex> lock(_mutex);
    //收集活跃连接round-robin选一个
    std::vector<std::shared_ptr<TcpConnection>> active;
    active.reserve(_connections.size());
    for (const auto& [id, conn] : _connections) {
        if (conn && conn->isConnected()) {
            active.push_back(conn);
        }
    }
    if (active.empty()) return nullptr;
    size_t idx = _pick_idx.fetch_add(1) % active.size();
    return active[idx];
}

std::shared_ptr<TcpConnection> TcpClient::GetConnection(ConnectionID connID) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _connections.find(connID);
    return (it != _connections.end()) ? it->second : nullptr;
}

std::shared_ptr<TcpConnection> TcpClient::GetConnection(const InetAddr& addr) const {
    std::lock_guard<std::mutex> lock(_mutex);
    for (const auto& [id, conn] : _connections) {
        if (conn && conn->PeerAddr() == addr) {
            return conn;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<TcpConnection>> TcpClient::getAllConnections() const {
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<std::shared_ptr<TcpConnection>> result;
    result.reserve(_connections.size());
    for (const auto& [id, conn] : _connections) {
        if (conn) result.push_back(conn);
    }
    return result;
}

} // namespace NetWork
