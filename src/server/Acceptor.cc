#include "Acceptor.h"
#include "TcpSocket.hpp"
#include "InetAddr.hpp"
#include <netinet/tcp.h>

namespace NetWork{

    Acceptor::Acceptor(int port, EventLoop *loop, bool defer_accept)
    :_listenSock(),
    _loop(loop),
    _channel(loop, -1),
    _idleFd(::open("/dev/null", O_RDONLY | O_CLOEXEC))
    {
        _listenSock.create_server(InetAddr(port));
        if (defer_accept) {
            // TCP_DEFER_ACCEPT: 只有当连接上有数据到达时才唤醒 accept
            int val = 1;
            ::setsockopt(_listenSock.Fd(), IPPROTO_TCP, TCP_DEFER_ACCEPT, &val, sizeof(val));
        }
        _channel.SetFd(_listenSock.Fd());
        _channel.SetReadCallback([this](){ OnRead_listenfd(); });
    }
    Acceptor::~Acceptor(){
        // loop 已停止, 直接移除 channel (不走 Channel::Remove, 其 assert isInLoopThread)
        _loop->RemoveEvent(&_channel);
        if (_idleFd >= 0) ::close(_idleFd);
        _listenSock.Close();
    }


    void Acceptor::SetAcceptCallback(const ServerCallback& cb){
        _acceptCallback = cb;
    }
    void Acceptor::accept(){
        //使用 LT 模式，避免 ET 模式下并发连接丢失的竞态条件
        _loop->runInLoop([this] { _channel.EnableRead(); });
    }
    void Acceptor::Stop(){
        _loop->runInLoop([this] { _channel.Remove(); });
    }
    void Acceptor::OnRead_listenfd(){
        while(true){
            InetAddr peer;
            int fd = _listenSock.accept(&peer);
            if(fd < 0){
                // 错误处理
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 非阻塞模式下没有更多连接，退出循环
                    break;
                } else if (errno == EINTR) {
                    // 被信号中断，继续尝试
                    continue;
                } else if (errno == ECONNABORTED) {
                    // 连接被客户端中止，继续尝试
                    continue;
                } else if (errno == EMFILE) {
                    // 进程 fd 耗尽：释放预留 fd → accept → 立即关闭 → 重新预留
                    // 这样可以打破"fd 耗尽 → 无法 accept → 无法释放旧连接"的死锁
                    ::close(_idleFd);
                    _idleFd = ::accept(_listenSock.Fd(), nullptr, nullptr);
                    if (_idleFd >= 0) ::close(_idleFd);
                    _idleFd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
                    if (_idleFd < 0) {
                        LOG(ERROR) << "Failed to reopen idle fd after EMFILE";
                    }
                    LOG(WARN) << "EMFILE: fd exhausted, accepted and closed one connection";
                } else {
                    // 其他错误
                    LOG(ERROR) << "accept failed: " << std::strerror(errno) << " (errno: " << errno << ")";
                    break;
                }
            } else {
                if(_acceptCallback){
                    _acceptCallback(fd, peer);
                } else {
                    LOG(ERROR) << "acceptCallback not set! connection from " << peer.StrAddr() << " will be closed";
                    ::close(fd);  // 回调未设置时关闭 fd，防止泄漏
                }
            }
        }
    }

}
