#include "Connector.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>

namespace NetWork {



const int Connector::kInitRetryDelayMs;
const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddr& serverAddr)
    : _loop(loop),
      _serverAddr(serverAddr),
      _connect(false),
      _state(kDisconnected),
      _retryDelayMs(kInitRetryDelayMs)
{
}

Connector::~Connector() {
    // 确保 Channel 已被清理（stopInLoop 会处理）
}

void Connector::start() {
    _connect.store(true, std::memory_order_relaxed);
    _loop->runInLoop([this]() { startInLoop(); });
}

// 判断是否为自连接（客户端连接到自身）
static bool isSelfConnect(int sockfd) {
    struct sockaddr_in localAddr, peerAddr;
    socklen_t addrlen = sizeof(localAddr);
    if (::getsockname(sockfd, (struct sockaddr*)&localAddr, &addrlen) < 0)
        return false;
    if (::getpeername(sockfd, (struct sockaddr*)&peerAddr, &addrlen) < 0)
        return false;
    return localAddr.sin_port == peerAddr.sin_port
        && localAddr.sin_addr.s_addr == peerAddr.sin_addr.s_addr;
}

void Connector::startInLoop() {
    assert(_loop->isInLoopThread());
    if (_state.load(std::memory_order_relaxed) != kDisconnected) return;
    if (_connect.load(std::memory_order_relaxed)) {
        connect();
    }
}

void Connector::stop() {
    _connect.store(false, std::memory_order_relaxed);
    _loop->queueTask([this]() { stopInLoop(); });
}

void Connector::stopInLoop() {
    assert(_loop->isInLoopThread());
    if (_state.load(std::memory_order_relaxed) == kConnecting) {
        setState(kDisconnected);
        int sockfd = removeAndResetChannel();
        if (sockfd >= 0) {
            ::close(sockfd);
        }
    }
}

void Connector::restart() {
    assert(_loop->isInLoopThread());
    setState(kDisconnected);
    _retryDelayMs = kInitRetryDelayMs;
    _connect.store(true, std::memory_order_relaxed);
    startInLoop();
}

void Connector::connect() {
    // 创建非阻塞 socket
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0) {
        LOG(ERROR) << "Connector::connect socket create failed: " << std::strerror(errno);
        return;
    }

    // 发起非阻塞连接
    const sockaddr_in* addr = _serverAddr.GetAddr();
    int ret = ::connect(sockfd, (const struct sockaddr*)addr, sizeof(sockaddr_in));
    int savedErrno = (ret == 0) ? 0 : errno;

    switch (savedErrno) {
        case 0:
        case EINPROGRESS:   // 非阻塞正常返回，连接尚未完成
        case EINTR:
        case EISCONN:       // 连接已建立
            connecting(sockfd);
            break;

        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:  // 连接被拒绝
        case ENETUNREACH:   // 网络不可达
            retry(sockfd);
            break;

        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            LOG(ERROR) << "Connector::connect error: " << std::strerror(savedErrno)
                       << " (errno: " << savedErrno << ")";
            ::close(sockfd);
            break;

        default:
            LOG(ERROR) << "Unexpected error in Connector::connect: "
                       << std::strerror(savedErrno) << " (errno: " << savedErrno << ")";
            ::close(sockfd);
            break;
    }
}

void Connector::connecting(int sockfd) {
    setState(kConnecting);
    _channel.reset(new Channel(_loop, sockfd));
    // 使用 weak_ptr 捕获，防止 Connector 被销毁后回调悬空
    std::weak_ptr<Connector> weak_self = shared_from_this();
    _channel->SetWriteCallback([weak_self]() {
        auto self = weak_self.lock();
        if (self) self->handleWrite();
    });
    _channel->SetErrorCallback([weak_self]() {
        auto self = weak_self.lock();
        if (self) self->handleError();
    });
    _channel->EnableWriteET();
}

int Connector::removeAndResetChannel() {
    _channel->DisableAll();
    _channel->Remove();
    int sockfd = _channel->fd();
    // 不能立即重置 channel（可能正在 HandleEvent 中）
    _loop->queueTask([this]() { resetChannel(); });
    return sockfd;
}

void Connector::resetChannel() {
    _channel.reset();
}

void Connector::handleWrite() {
    if (_state.load(std::memory_order_relaxed) == kConnecting) {
        int sockfd = removeAndResetChannel();
        // 检查 SO_ERROR，确认连接是否成功
        int err = 0;
        socklen_t len = sizeof(err);
        if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
            err = errno;
        }
        if (err != 0) {
            LOG(WARN) << "Connector::handleWrite SO_ERROR: " << std::strerror(err)
                      << " (" << err << ")";
            retry(sockfd);
        } else if (isSelfConnect(sockfd)) {
            LOG(WARN) << "Connector::handleWrite - Self connect detected";
            retry(sockfd);
        } else {
            setState(kConnected);
            if (_connect.load(std::memory_order_relaxed)) {
                _newConnectionCallback(sockfd);
            } else {
                ::close(sockfd);
            }
        }
    }
}

void Connector::handleError() {
    LOG(ERROR) << "Connector::handleError state=" << (int)_state.load(std::memory_order_relaxed);
    if (_state.load(std::memory_order_relaxed) == kConnecting) {
        int sockfd = removeAndResetChannel();
        int err = 0;
        socklen_t len = sizeof(err);
        if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
            err = errno;
        }
        LOG(TRACE) << "SO_ERROR = " << err << " " << std::strerror(err);
        retry(sockfd);
    }
}

void Connector::retry(int sockfd) {
    ::close(sockfd);
    setState(kDisconnected);
    if (_connect.load(std::memory_order_relaxed)) {
        LOG(INFO) << "Retry connecting to " << _serverAddr.StrAddr()
                  << " in " << _retryDelayMs << "ms";
        _loop->AddTimedTask((_retryDelayMs + 999) / 1000,
            [self = shared_from_this()]() { self->startInLoop(); });
        _retryDelayMs = std::min(_retryDelayMs * 2, kMaxRetryDelayMs);
    }
}

} // namespace NetWork
