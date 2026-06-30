#pragma once

#include "../common/EventLoop.h"
#include "../common/InetAddr.hpp"
#include "../common/noncopyable.hpp"
#include "Connector.h"
#include "../common/TcpConnection.h"
#include "../common/LoopThreadPool.h"

#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <atomic>
#include <vector>

namespace NetWork {

    using std::unordered_map;

    // TcpClient 异步TCP客户端, 单连是多连的特例
    class TcpClient : public noncopyable {
    public:
        using LoadBalanceCallback = NetWork::LoadBalanceCallback;

        //单host构造
        TcpClient(EventLoop* loop, const InetAddr& serverAddr, const std::string& name = "TcpClient");
        //多host构造
        TcpClient(EventLoop* loop, const std::vector<InetAddr>& serverAddrs, const std::string& name = "TcpClient");
        ~TcpClient();

        //发起连接
        void connect();
        //断开所有连接
        void disconnect();
        //幂等停止, 不退出_loop
        void stop();

        // ===== 查找连接 =====
        //round-robin选活跃连接
        std::shared_ptr<TcpConnection> GetConnection() const;
        //按ConnectionID查找
        std::shared_ptr<TcpConnection> GetConnection(ConnectionID connID) const;
        //按目标地址查找
        std::shared_ptr<TcpConnection> GetConnection(const InetAddr& addr) const;
        //获取所有连接
        std::vector<std::shared_ptr<TcpConnection>> getAllConnections() const;

        EventLoop* getLoop() const { return _loop; }
        bool retry() const { return _retry; }
        void enableRetry()  { _retry = true; }
        void disableRetry() { _retry = false; }
        const std::string& name() const { return _name; }

        //设置IO线程数, connect前调用
        void setExtraThread(int count);
        void setLoadBalancer(LoadBalanceCallback cb) { _load_balancer = std::move(cb); }

        //设置回调
        void setConnectionCallback(const AppLayerCallback& cb) { _connected_cb = cb; }
        void setMessageCallback(const AppLayerCallback& cb) { _message_cb = cb; }
        void setWriteCompleteCallback(const AppLayerCallback& cb) { _writeComplete_cb = cb; }

    private:
        //Connector回调, 在_loop线程
        void newConnection(int sockfd, std::shared_ptr<Connector> connector);
        //连接关闭回调, 在IO loop线程
        void removeConnection(ConnectionID connID, std::shared_ptr<Connector> connector);
        //负载均衡选IO loop
        EventLoop* getNextLoop();

        EventLoop* _loop;                            //外部传入, 运行connector
        std::string _name;
        std::atomic<bool> _started{false};
        std::atomic<bool> _retry{false};             //自动重连
        std::atomic<bool> _connect{false};           //用户期望连接
        std::atomic<ConnectionID> _nextid{0};        //连接ID自增

        std::vector<InetAddr> _server_addrs;         //目标地址列表
        std::vector<std::shared_ptr<Connector>> _connectors;  //持有所有connector
        unordered_map<ConnectionID, std::shared_ptr<TcpConnection>> _connections;  //管理所有连接
        mutable std::mutex _mutex;                   //保护_connections

        LoopThreadPool _loop_thread_pool;            //IO线程池
        LoadBalanceCallback _load_balancer;
        std::atomic<size_t> _next_loop_idx{0};
        mutable std::atomic<size_t> _pick_idx{0};

        AppLayerCallback _message_cb;
        AppLayerCallback _connected_cb;
        AppLayerCallback _writeComplete_cb;
    };

} // namespace NetWork
