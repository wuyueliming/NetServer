#include"TcpServer.h"
#include "TcpConnection.h"
#include "TimeWheel.h"
#include <csignal>


namespace NetWork{

    TcpServer::TcpServer(EventLoop* loop, int port, const std::string& name)
    :_loop(loop),
    _name(name),
    _addr(port, "0.0.0.0"),
    _nextid(0),
    _timeout(0),
    _loop_thread_pool(),
    _acceptor(std::make_unique<Acceptor>(port, _loop, false)),
    _connections()
    {
        _loop_thread_pool.SetThreadCount(0);  // 默认不创建额外线程，主loop同时accept+IO
        _acceptor->SetAcceptCallback([this](int fd, InetAddr addr){ AcceptConnection(fd, addr); });
    }

    TcpServer::~TcpServer(){
        stop();
    }

    void TcpServer::start(){
        if (_started.load()) return;
        _started.store(true);

        // 1. 创建从 EventLoop 线程池
        _loop_thread_pool.Create();

        // 2. 如果启用了 defer_accept，需要重建 Acceptor
        if (_defer_accept) {
            _acceptor = std::make_unique<Acceptor>(_addr.HostPort(), _loop, true);
            _acceptor->SetAcceptCallback([this](int fd, InetAddr addr){ AcceptConnection(fd, addr); });
        }
        _acceptor->accept();
        // 由用户自行调用 _loop->loop()
    }

    void TcpServer::stop(){
        if (!_started.load()) return;
        _started.store(false);  // 先置 false, 防止 signal handler 重入

        // 1. 停止接受新连接
        if (_acceptor) _acceptor->Stop();

        // 2. 在主 EventLoop 线程中同步释放所有连接
        //    swap 出 _connections, 避免迭代时 erase 导致迭代器失效
        _loop->runInLoop([this](){
            std::unordered_map<ConnectionID, std::shared_ptr<TcpConnection>> conns;
            conns.swap(_connections);
            for (auto &pair : conns) {
                pair.second->Release();
            }
        });

        //3. 退出主EventLoop
        _loop->Quit();
    }

    //设置应用层回调函数
    void TcpServer::setMessageCallback(const AppLayerCallback& cb){
        _message_cb = cb;
    }
    void TcpServer::setConnectionCallback(const AppLayerCallback& cb){
        _connected_cb = cb;
    }
    void TcpServer::setCloseCallback(const AppLayerCallback& cb){
        _closed_cb = cb;
    }
    void TcpServer::setEventCallback(const AppLayerCallback& cb){
        _event_cb = cb;
    }
    void TcpServer::setWriteCompleteCallback(const AppLayerCallback& cb){
        _writeComplete_cb = cb;
    }

    //管理连接的回调
    void TcpServer::AcceptConnection(int fd, InetAddr addr){
        ConnectionID connID = _nextid.fetch_add(1);
        EventLoop* nextLoop = _loop_thread_pool.NextLoop();
        if (!nextLoop) nextLoop = _loop;  // 线程数为0时回退到主 EventLoop
        auto connPtr = std::make_shared<TcpConnection>(nextLoop, connID, fd, addr);
        // 投递到 EventLoop 操作 _connections，避免跨线程竞争
        _loop->runInLoop([this, connID, connPtr](){
            _connections.insert({connID, connPtr});
        });
        LOG(INFO)<<"Accept Connection connID:"<<connID<<" addr:"<<addr.StrAddr();
        //注册server内部的关闭回调
        connPtr->SetReleaseCallback([this](ConnectionID id, EventLoop* loop){ ReleaseConnection(id, loop); });
        //注册上层功能回调
        connPtr->SetMessageCallback(_message_cb);
        connPtr->SetConnectedCallback(_connected_cb);
        connPtr->SetClosedCallback(_closed_cb);
        connPtr->SetEventCallback(_event_cb);
        if (_writeComplete_cb) {
            connPtr->SetWriteCompleteCallback(_writeComplete_cb);
        }
        //设置超时销毁
        if(_timeout > 0){
            connPtr->EnableInactiveRelease(_timeout);
        }
        //启动Connection监听事件
        connPtr->Established();

    }
    void TcpServer::ReleaseConnection(ConnectionID connID, EventLoop *loop){
        _loop->runInLoop([this, connID](){ ReleaseConnectionInLoop(connID); });
    }
    void TcpServer::ReleaseConnectionInLoop(ConnectionID connID){
        auto  it = _connections.find(connID);
        if(it == _connections.end()){
            LOG(ERROR)<<"ReleaseConnection connID:"<<connID<<" not found";
            return;
        }
        LOG(INFO)<<"Release Connection In Server connID:"<<connID;
        _connections.erase(it);
    }

    //定时任务
    void TcpServer::AddTimedTask(TimedTask task, uint32_t delay){
        _loop->AddTimedTask(delay,std::move(task));
    }

    //超时销毁
    void TcpServer::enableInactiveRelease(uint32_t timeout){
        _timeout = timeout;
    }
    void TcpServer::disableInactiveRelease(){
        _timeout = 0;
    }

    void TcpServer::setExtraThread(int count){
        _loop_thread_pool.SetThreadCount(count);
    }

    void TcpServer::setThreadNamePrefix(const std::string& prefix){
        _loop_thread_pool.SetThreadNamePrefix(prefix);
    }

    void TcpServer::setLoadBalancer(LoadBalanceCallback cb){
        _loop_thread_pool.SetLoadBalancer(std::move(cb));
    }

    EventLoop* TcpServer::getNextLoop(){
        auto& loops = _loop_thread_pool.GetAllLoops();
        if (loops.empty()) return _loop;
        // 负载均衡策略，返回负载最小的 EventLoop
        return _loop_thread_pool.NextLoop();
    }

    void TcpServer::migrateConnection(ConnectionID connID, EventLoop *target_loop){
        _loop->runInLoop([this, connID, target_loop](){
            auto it = _connections.find(connID);
            if (it == _connections.end()) return;
            it->second->MigrateTo(target_loop);
        });
    }

    void TcpServer::forEachConnection(std::function<void(const std::shared_ptr<TcpConnection>&)> cb){
        _loop->runInLoop([this, cb = std::move(cb)](){
            for (auto& pair : _connections) {
                cb(pair.second);
            }
        });
    }

    std::shared_ptr<TcpConnection> TcpServer::GetConnection(ConnectionID connID) const {
        if (!_started.load()) return nullptr;
        // loop 线程内直接读
        if (_loop->isInLoopThread()) {
            auto it = _connections.find(connID);
            return it != _connections.end() ? it->second : nullptr;
        }
        // 跨线程投递到 loop 线程获取
        std::promise<std::shared_ptr<TcpConnection>> prom;
        auto fut = prom.get_future();
        _loop->runInLoop([this, connID, &prom](){
            auto it = _connections.find(connID);
            prom.set_value(it != _connections.end() ? it->second : nullptr);
        });
        return fut.get();
    }

}
