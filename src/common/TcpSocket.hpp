#pragma once

#include "Socket.hpp"
#include <netinet/tcp.h>

namespace NetWork {

// TcpSocket：TCP 专属功能
// create(SOCK_STREAM) / listen / accept / connect / recv / send / SetDeferAccept
class TcpSocket : public Socket {
    static const int MAX_BACKLOG = 1024;
public:
    TcpSocket() : Socket(-1) {}
    TcpSocket(int fd) : Socket(fd) {}
    ~TcpSocket() = default;

    bool create() {
        _sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (_sockfd == -1) {
            LOG(ERROR) << "tcp socket create failed: " << std::strerror(errno) << " (errno: " << errno << ")";
            return false;
        }
        return true;
    }

    // TCP 专属选项：TCP_DEFER_ACCEPT
    void SetDeferAccept(int timeout_sec = 0) {
        int val = timeout_sec > 0 ? timeout_sec : 1;
        ::setsockopt(_sockfd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &val, sizeof(val));
    }

    // 服务端：创建监听套接字
    bool create_server(const InetAddr& addr, bool nonblock = true) {
        if (_sockfd == -1) {
            if (!create()) return false;
        }
        SetReuseAddr();
        SetReusePort();
        if (!bind(addr)) return false;
        if (nonblock) SetNonBlock();
        if (!listen()) return false;
        return true;
    }

    bool listen(int backlog = MAX_BACKLOG) {
        int ret = ::listen(_sockfd, backlog);
        if (ret == -1) {
            LOG(ERROR) << "listen failed on fd " << _sockfd << ": " << std::strerror(errno) << " (errno: " << errno << ")";
            return false;
        }
        return true;
    }

    int accept(InetAddr* addr = nullptr) {
        socklen_t len = sizeof(sockaddr_in);
        int fd = -1;
        if (addr != nullptr) {
            fd = ::accept(_sockfd, (sockaddr*)addr->GetAddr(), &len);
        } else {
            fd = ::accept(_sockfd, nullptr, nullptr);
        }
        return fd;
    }

    // 客户端
    bool create_client(InetAddr peer) {
        if (_sockfd == -1) {
            if (!create()) return false;
        }
        if (!connect(peer)) return false;
        return true;
    }

    bool connect(const InetAddr& addr) {
        int ret = ::connect(_sockfd, (const sockaddr*)addr.GetAddr(), sizeof(sockaddr_in));
        if (ret == -1) {
            LOG(ERROR) << "connect failed to " << addr.StrAddr() << ": " << std::strerror(errno) << " (errno: " << errno << ")";
            return false;
        }
        return true;
    }

    // TCP I/O
    ssize_t recv(char* buf, size_t len, int flags = 0) {
        return ::recv(_sockfd, buf, len, flags);
    }

    ssize_t send(const char* buf, size_t len, int flags = 0) {
        return ::send(_sockfd, buf, len, flags);
    }
};

}