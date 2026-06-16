#include"TcpServer.h"
#include "TcpConnection.h"
#include "TimeWheel.h"
#include "SignalHandler.h"
#include <csignal>


namespace Aether{

    // 统一构造函数：主 Reactor 直接在构造中创建 Reactor
    // 后续在 start() 中由主线程调用 _main_reactor.loop() 阻塞主线程
    TcpServer::TcpServer(int port)
    :_addr(port, "0.0.0.0"),
    _nextid(0),
    _timeout(0),
    _main_reactor(),
    _thread_pool(&_main_reactor),
    _acceptor(std::make_unique<Acceptor>(port, &_main_reactor, false)),
    _connections()
    {
        _thread_pool.SetThreadCount(kDefaultThreadNum);
        _acceptor->SetAcceptCallback([this](int fd, InetAddr addr){ AcceptConnection(fd, addr); });
        // 在创建线程前阻塞信号，由 Reactor 的 SignalHandler 统一处理
        SignalHandler::BlockSignals({SIGINT, SIGTERM});
        signal(SIGPIPE, SIG_IGN);
    }

    TcpServer::~TcpServer(){
        stop();
    }

    void TcpServer::start(){
        _thread_pool.Create();
        // 如果启用了 defer_accept，需要重建 Acceptor
        if (_defer_accept) {
            _acceptor = std::make_unique<Acceptor>(_addr.HostPort(), &_main_reactor, true);
            _acceptor->SetAcceptCallback([this](int fd, InetAddr addr){ AcceptConnection(fd, addr); });
        }
        _acceptor->accept();
        // 主线程阻塞在事件循环中，由 SignalHandler 的信号回调调用 stop() 退出
        _main_reactor.loop();
    }

    void TcpServer::stop(){
        //1.停止接受新连接
        if (_acceptor) _acceptor->Stop();
        //2.在主 Reactor 线程中安全收集并释放所有连接
        _main_reactor.runInLoop([this](){
            for (auto &pair : _connections) {
                pair.second->Release();
            }
        });
        //3.停止从reactor线程池
        _thread_pool.Stop();
        //4.退出主reactor事件循环
        _main_reactor.stop();
    }

    //设置应用层回调函数
    void TcpServer::SetMessageCallback(const AppLayerCallback& cb){
        _message_cb = cb;
    }
    void TcpServer::SetConnectedCallback(const AppLayerCallback& cb){
        _connected_cb = cb;
    }
    void TcpServer::SetClosedCallback(const AppLayerCallback& cb){
        _closed_cb = cb;
    }
    void TcpServer::SetEventCallback(const AppLayerCallback& cb){
        _event_cb = cb;
    }
    void TcpServer::SetHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t mark){
        _highWaterMark_cb = cb;
        _highWaterMark = mark;
    }
    void TcpServer::SetWriteCompleteCallback(const WriteCompleteCallback& cb){
        _writeComplete_cb = cb;
    }

    //管理连接的回调
    void TcpServer::AcceptConnection(int fd, InetAddr addr){
        ConnectionID connID = _nextid.fetch_add(1);
        auto connPtr = std::make_shared<TcpConnection>(_thread_pool.NextReactor(), connID, fd, addr);
        // 投递到主 Reactor 操作 _connections，避免跨线程竞争
        _main_reactor.runInLoop([this, connID, connPtr](){
            _connections.insert({connID, connPtr});
        });
        LOG(INFO)<<"Accept Connection connID:"<<connID<<" addr:"<<addr.StrAddr();
        //注册server内部的关闭回调
        connPtr->SetReleaseCallback([this](ConnectionID id, Reactor* loop){ ReleaseConnection(id, loop); });
        //注入帧解码器（每个连接创建独立实例）
        if (_frame_decoder_factory) {
            connPtr->SetFrameDecoder(_frame_decoder_factory());
        }
        //注册上层功能回调
        connPtr->SetMessageCallback(_message_cb);
        connPtr->SetConnectedCallback(_connected_cb);
        connPtr->SetClosedCallback(_closed_cb);
        connPtr->SetEventCallback(_event_cb);
        if (_highWaterMark_cb) {
            connPtr->SetHighWaterMarkCallback(_highWaterMark_cb, _highWaterMark);
        }
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
    void TcpServer::ReleaseConnection(ConnectionID connID, Reactor *loop){
        _main_reactor.runInLoop([this, connID](){ ReleaseConnectionInLoop(connID); });
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
        _main_reactor.AddTimedTask(delay,std::move(task));
    }

    //超时销毁
    void TcpServer::EnableInactiveRelease(uint32_t timeout){
        _timeout = timeout;
    }
    void TcpServer::DisableInactiveRelease(){
        _timeout = 0;
    }

    void TcpServer::SetThreadCount(int count){
        _thread_pool.SetThreadCount(count);
    }

    Reactor* TcpServer::GetNextLoop(){
        auto& reactors = _thread_pool.GetAllReactors();
        if (reactors.empty()) return &_main_reactor;
        // 简单 Round-Robin，返回下一个 Reactor
        return _thread_pool.NextReactor();
    }

    void TcpServer::MigrateConnection(ConnectionID connID, Reactor *target_loop){
        _main_reactor.runInLoop([this, connID, target_loop](){
            auto it = _connections.find(connID);
            if (it == _connections.end()) return;
            it->second->MigrateTo(target_loop);
        });
    }

}
