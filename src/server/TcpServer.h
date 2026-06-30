#pragma once

#include<unordered_map>
#include<vector>
#include<memory>
#include<atomic>
#include<thread>
#include<future>
#include "ServerBase.h"
#include "../common/TimeWheel.h"
#include "../common/InetAddr.hpp"
#include "Acceptor.h"
#include "../common/TcpConnection.h"
#include "../common/EventLoop.h"
#include "../common/LoopThreadPool.h"
#include "../common/noncopyable.hpp"



namespace NetWork{

    using std::unordered_map;

    static constexpr int kDefaultThreadNum = 5;
    class TcpServer : public ServerBase, public noncopyable{
    public:
        explicit TcpServer(EventLoop* loop, int port, const std::string& name = "TcpServer");
        ~TcpServer();

        // 用户自行调用 _loop->loop()
        void start() override;
        // stop() 停止监听并退出 EventLoop (幂等)
        void stop() override;

        void setThreadNum(int count) override;
        void setThreadNamePrefix(const std::string& prefix);
        void setLoadBalancer(LoadBalanceCallback cb);         // 设置自定义负载均衡策略
        //添加定时任务
        void AddTimedTask(TimedTask task, uint32_t delay);
        //设置超时销毁
        void enableInactiveRelease(uint32_t timeout) override;
        void disableInactiveRelease() override;
        //设置应用层回调函数
        void setMessageCallback(const AppLayerCallback& cb) override;
        void setConnectionCallback(const AppLayerCallback& cb) override;
        void setCloseCallback(const AppLayerCallback& cb) override;
        void setEventCallback(const AppLayerCallback& cb) override;
        void setWriteCompleteCallback(const AppLayerCallback& cb);
        //启用 TCP_DEFER_ACCEPT（减少无数据连接的 EPOLLIN 唤醒）
        void enableDeferAccept() { _defer_accept = true; }
        //连接迁移：将连接迁移到目标 EventLoop
        void migrateConnection(ConnectionID connID, EventLoop *target_loop);
        //获取下一个 EventLoop（负载均衡策略）
        EventLoop* getNextLoop();
        //遍历所有连接（在 EventLoop 线程中安全执行）
        void forEachConnection(std::function<void(const std::shared_ptr<TcpConnection>&)> cb);
        //按 ConnectionID 查找连接（线程安全，只读）
        std::shared_ptr<TcpConnection> GetConnection(ConnectionID connID) const;
        //获取当前 EventLoop
        EventLoop* getLoop() const { return _loop; }
        //获取名称
        const std::string& name() const { return _name; }
    private:
        //管理连接的回调,注册给acceptor和connection
        void AcceptConnection(int fd, InetAddr addr);
        void ReleaseConnection(ConnectionID connID, EventLoop *loop);
        void ReleaseConnectionInLoop(ConnectionID connID);/*InLoop:被connection调用，用于释放连接*/
    private:
        EventLoop* _loop;                          // 外部传入的 EventLoop，不拥有所有权
        std::atomic<bool> _started{false};         // 是否已启动
        std::string _name;                        // 服务器名称
        InetAddr _addr;
        std::atomic<ConnectionID> _nextid;//自动增长的连接id（原子操作，线程安全）
        uint32_t _timeout;//超时销毁时间,单位秒,默认0表示不开启超时销毁

        LoopThreadPool _loop_thread_pool;             // 从 EventLoop 线程池
        std::unique_ptr<Acceptor> _acceptor;     // 监听套接字->获取新连接
        bool _defer_accept = false;
        unordered_map<ConnectionID,std::shared_ptr<TcpConnection>> _connections;//管理所有连接,生命周期取决于server
        //应用层回调函数
        AppLayerCallback _message_cb;//消息回调函数
        AppLayerCallback _connected_cb;//连接成功回调函数
        AppLayerCallback _closed_cb;//连接关闭回调函数
        AppLayerCallback _event_cb;//自定义任意事件回调函数
        AppLayerCallback _writeComplete_cb;
    };

  

}
