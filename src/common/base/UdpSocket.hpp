#pragma once

#include "Socket.hpp"

namespace Aether {

// UdpSocket：UDP 专属功能
// create(SOCK_DGRAM) / RecvFrom / SendTo / create_udp_server
class UdpSocket : public Socket {
public:
    UdpSocket() : Socket(-1) {}
    ~UdpSocket() = default;

    bool create() {
        _sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (_sockfd == -1) {
            LOG(ERROR) << "udp socket create failed: " << std::strerror(errno) << " (errno: " << errno << ")";
            return false;
        }
        return true;
    }

    // 服务端：创建 UDP 套接字（无 listen）
    bool create_udp_server(const InetAddr& addr, bool nonblock = true) {
        if (_sockfd == -1) {
            if (!create()) return false;
        }
        SetReuseAddr();
        SetReusePort();
        if (!bind(addr)) return false;
        if (nonblock) SetNonBlock();
        return true;
    }

    // UDP I/O
    ssize_t RecvFrom(char* buf, size_t len, InetAddr* peer = nullptr) {
        if (peer != nullptr) {
            socklen_t addrlen = sizeof(sockaddr_in);
            return ::recvfrom(_sockfd, buf, len, 0, (sockaddr*)peer->GetAddr(), &addrlen);
        } else {
            return ::recvfrom(_sockfd, buf, len, 0, nullptr, nullptr);
        }
    }

    ssize_t SendTo(const char* buf, size_t len, const InetAddr& peer) {
        return ::sendto(_sockfd, buf, len, 0, (const sockaddr*)peer.GetAddr(), sizeof(sockaddr_in));
    }
};

}