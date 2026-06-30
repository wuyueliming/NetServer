#include "Channel.h"
#include "EventLoop.h"
#include <cassert>

namespace NetWork {
    void Channel::Remove(){ assert(_loop->isInLoopThread()); _loop->RemoveEvent(this); }
    void Channel::Update(){ assert(_loop->isInLoopThread()); _loop->UpdateEvent(this); }

    void Channel::EnableRead() {
        assert(_loop->isInLoopThread());
        _events |= EPOLLIN;
        Update();
    }

    void Channel::EnableReadET() {
        assert(_loop->isInLoopThread());
        _events |= EPOLLIN | EPOLLET | EPOLLRDHUP;
        Update();
    }

    void Channel::EnableWrite() {
        assert(_loop->isInLoopThread());
        _events |= EPOLLOUT;
        Update();
    }

    void Channel::EnableWriteET() {
        assert(_loop->isInLoopThread());
        _events |= EPOLLOUT | EPOLLET;
        Update();
    }

    void Channel::DisableRead() {
        assert(_loop->isInLoopThread());
        _events &= ~EPOLLIN;
        Update();
    }

    void Channel::DisableWrite() {
        assert(_loop->isInLoopThread());
        _events &= ~EPOLLOUT;
        Update();
    }

    void Channel::DisableAll() {
        assert(_loop->isInLoopThread());
        _events = 0;
        Update();
    }
}
