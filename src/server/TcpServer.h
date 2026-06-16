#pragma once

#include<unordered_map>
#include<vector>
#include<memory>
#include<atomic>
#include "ServerBase.h"
#include "TimeWheel.h"
#include "base/InetAddr.hpp"
#include "Acceptor.h"
#include "TcpConnection.h"
#include "Reactor.h"
#include "ThreadPool.h"
#include "base/noncopyable.hpp"



namespace Aether{

    using std::unordered_map;

    static constexpr int kDefaultThreadNum = 5;
    class TcpServer : public ServerBase, public noncopyable{
    public:
        explicit TcpServer(int port);
        ~TcpServer();

        void start() override;
        void stop() override;
        void SetThreadCount(int count) override;
        //添加定时任务
        void AddTimedTask(TimedTask task, uint32_t delay);
        //设置超时销毁
        void EnableInactiveRelease(uint32_t timeout) override;
        void DisableInactiveRelease() override;
        //设置应用层回调函数
        void SetMessageCallback(const AppLayerCallback& cb) override;
        void SetConnectedCallback(const AppLayerCallback& cb) override;
        void SetClosedCallback(const AppLayerCallback& cb) override;
        void SetEventCallback(const AppLayerCallback& cb) override;
        void SetHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t mark);
        void SetWriteCompleteCallback(const WriteCompleteCallback& cb);
        //启用 TCP_DEFER_ACCEPT（减少无数据连接的 EPOLLIN 唤醒）
        void EnableDeferAccept() { _defer_accept = true; }
        //设置帧解码器工厂（每个连接创建独立的解码器实例）
        void SetFrameDecoderFactory(FrameDecoderFactory factory) override { _frame_decoder_factory = std::move(factory); }
        //连接迁移：将连接迁移到目标 Reactor
        void MigrateConnection(ConnectionID connID, Reactor *target_loop);
        //获取下一个从 Reactor（Round-Robin）
        Reactor* GetNextLoop();
    private:
        //管理连接的回调,注册给acceptor和connection
        void AcceptConnection(int fd, InetAddr addr);
        void ReleaseConnection(ConnectionID connID, Reactor *loop);
        void ReleaseConnectionInLoop(ConnectionID connID);/*InLoop:被connection调用，用于释放连接*/
    private:
        InetAddr _addr;
        std::atomic<ConnectionID> _nextid;//自动增长的连接id（原子操作，线程安全）
        uint32_t _timeout;//超时销毁时间,单位秒,默认0表示不开启超时销毁

        Reactor _main_reactor;                   // 主 Reactor（无线程，由 start() 中主线程 loop()）
        ThreadPool _thread_pool;             // 从 reactor 线程池
        std::unique_ptr<Acceptor> _acceptor;     // 监听套接字->获取新连接
        bool _defer_accept = false;
        FrameDecoderFactory _frame_decoder_factory;  // 帧解码器工厂（每个连接创建独立实例）
        unordered_map<ConnectionID,std::shared_ptr<TcpConnection>> _connections;//管理所有连接,生命周期取决于server
        //应用层回调函数
        AppLayerCallback _message_cb;//消息回调函数
        AppLayerCallback _connected_cb;//连接成功回调函数
        AppLayerCallback _closed_cb;//连接关闭回调函数
        AppLayerCallback _event_cb;//自定义任意事件回调函数
        HighWaterMarkCallback _highWaterMark_cb;
        size_t _highWaterMark = 64 * 1024 * 1024;  // 默认 64MB
        WriteCompleteCallback _writeComplete_cb;
    };

    /*
    心得1：回调函数的bind及参数设计
    1.从上层角度看，底层的回调参数应该要满足上层的需求
    2.从下层角度看，下层可能执行时不知道需要什么参数，所以需要在回调函数中绑定必要的参数

    心得2：参数设计
    纯输入参数(只读)：const &
    输出输出参数：&
    纯输出参数(可写)：*
    */

}
