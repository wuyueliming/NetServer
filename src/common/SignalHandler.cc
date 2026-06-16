#include "SignalHandler.h"
#include "Reactor.h"
#include "base/Logger.hpp"
#include <sys/signalfd.h>
#include <unistd.h>
#include <cstring>

namespace Aether {

SignalHandler::SignalHandler(Reactor *loop)
: _loop(loop),
  _sigfd(-1),
  _channel(loop, -1)
{
    sigemptyset(&_mask);
}

SignalHandler::~SignalHandler() {
    if (_sigfd >= 0) {
        _channel.Remove();
        close(_sigfd);
        _sigfd = -1;
    }
}

void SignalHandler::Register(int signo, SignalCallback handler) {
    sigaddset(&_mask, signo);

    // 如果 signalfd 已创建，更新其信号掩码
    if (_sigfd >= 0) {
        if (signalfd(_sigfd, &_mask, SFD_NONBLOCK | SFD_CLOEXEC) < 0) {
            LOG(ERROR) << "signalfd update failed: " << std::strerror(errno);
            sigdelset(&_mask, signo);  // 回滚 mask
            return;
        }
    } else {
        // 首次注册，创建 signalfd
        int fd = signalfd(-1, &_mask, SFD_NONBLOCK | SFD_CLOEXEC);
        if (fd < 0) {
            LOG(ERROR) << "signalfd create failed: " << std::strerror(errno);
            sigdelset(&_mask, signo);  // 回滚 mask
            return;
        }
        _sigfd = fd;
        _channel.SetFd(_sigfd);
        _channel.SetReadCallback([this](){ OnRead(); });
        _channel.EnableRead();
    }

    // signalfd 创建/更新成功，才注册 handler 和阻塞信号
    _handlers[signo] = std::move(handler);
    pthread_sigmask(SIG_BLOCK, &_mask, nullptr);
}

void SignalHandler::BlockSignals(const std::vector<int>& signals) {
    sigset_t mask;
    sigemptyset(&mask);
    for (int signo : signals) {
        sigaddset(&mask, signo);
    }
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);
}

void SignalHandler::OnRead() {
    while (true) {
        struct signalfd_siginfo fdsi;
        ssize_t ret = read(_sigfd, &fdsi, sizeof(fdsi));
        if (ret != sizeof(fdsi)) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            LOG(ERROR) << "signalfd read failed: " << std::strerror(errno);
            return;
        }

        auto it = _handlers.find(fdsi.ssi_signo);
        if (it != _handlers.end()) {
            it->second();
        }
    }
}

}