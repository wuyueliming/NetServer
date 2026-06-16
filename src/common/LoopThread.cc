#include "LoopThread.h"

namespace Aether{

    LoopThread::LoopThread():_reactor(NULL), _thread(&LoopThread::Entry, this) {}

    Reactor *LoopThread::GetReactor() {
        Reactor *reactor = NULL;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [&](){ return _reactor != NULL; });
            reactor = _reactor;
        }
        return reactor;
    }

    Reactor *LoopThread::GetLoop() {
        return GetReactor()->GetLoop();
    }

    void LoopThread::Entry() {
        Reactor reactor;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _reactor = &reactor;
            _cond.notify_all();
        }
        reactor.loop();
    }

    void LoopThread::Stop() {
        Reactor *reactor = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            reactor = _reactor;
        }
        if (reactor) reactor->stop();
    }

    LoopThread::~LoopThread() {
        Stop();
        if (_thread.joinable()) _thread.join();
    }

}
