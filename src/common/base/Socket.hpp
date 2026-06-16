#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include "Logger.hpp"
#include "InetAddr.hpp"
#include "noncopyable.hpp"

namespace Aether {

// Socket 基类：仅包含通用的 fd 管理和 socket 选项
// TCP/UDP 专属功能分别由 TcpSocket/UdpSocket 实现
class Socket : public noncopyable {
public:
    Socket() : _sockfd(-1) {}
    Socket(int fd) : _sockfd(fd) {}
    virtual ~Socket() { Close(); }

    virtual int Fd() const { return _sockfd; }
    bool Close() {
        if (_sockfd == -1) return false;
        ::close(_sockfd);
        _sockfd = -1;
        return true;
    }

    // 释放 fd 所有权（不关闭），用于将 fd 转交给上层
    int ReleaseFd() {
        int fd = _sockfd;
        _sockfd = -1;
        return fd;
    }

    // 通用 socket 选项
    void SetNonBlock() {
        int flag = fcntl(_sockfd, F_GETFL, 0);
        if (flag < 0) return;
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }

    void SetReuseAddr() {
        int val = 1;
        ::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&val, sizeof(int));
    }

    void SetReusePort() {
        int val = 1;
        ::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void*)&val, sizeof(int));
    }

    // 通用操作：bind 对 TCP/UDP 都适用
    bool bind(const InetAddr& addr) {
        int ret = ::bind(_sockfd, (const sockaddr*)addr.GetAddr(), sizeof(sockaddr_in));
        if (ret == -1) {
            LOG(ERROR) << "bind failed on " << addr.StrAddr() << ": " << std::strerror(errno) << " (errno: " << errno << ")";
            return false;
        }
        return true;
    }

protected:
    int _sockfd;
};

}