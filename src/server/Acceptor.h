#pragma once

#include "../common/InetAddr.hpp"
#include "../common/Channel.h"
#include "../common/TcpSocket.hpp"
#include "../common/EventLoop.h"
#include "../common/noncopyable.hpp"
#include <functional>
#include <cassert>
#include <fcntl.h>


namespace NetWork{


    class Acceptor : public noncopyable{
        using ServerCallback = std::function<void(int fd, InetAddr addr)>;
    public:
        Acceptor(int port, EventLoop *loop, bool defer_accept = false);
        ~Acceptor();
        //1.设置accept新连接后的回调函数，用于server管理新连接
        void SetAcceptCallback(const ServerCallback& cb);
        //2.启动Acceptor监听事件
        void accept();
        //3.停止Acceptor监听
        void Stop();

    private:
        //处理读事件
        void OnRead_listenfd();
    private:
        //管理监听套接字
        TcpSocket _listenSock;
        EventLoop* _loop;
        Channel _channel;
        //server callback
        ServerCallback _acceptCallback;//accpet后在server中管理连接
        int _idleFd;  // 预留 fd，用于处理 EMFILE（fd 耗尽）
    };

}
