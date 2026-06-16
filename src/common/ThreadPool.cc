#include "ThreadPool.h"

namespace Aether{

    ThreadPool::ThreadPool(Reactor *masterreactor)
    :_thread_count(0), _next_idx(0), _masterreactor(masterreactor) {}

    void ThreadPool::SetThreadCount(int count) {
        _thread_count = count;
    }

    void ThreadPool::Create() {
        if (_thread_count > 0) {
            _loopThreads.resize(_thread_count);
            _slaverreactors.resize(_thread_count);
            for (size_t i = 0; i < _thread_count; i++) {
                _loopThreads[i] = std::make_unique<LoopThread>();
                _slaverreactors[i] = _loopThreads[i]->GetReactor();
            }
        }
        LOG(INFO) << "ThreadPool Created, thread count:" << _thread_count;
        return;
    }

    void ThreadPool::Stop() {
        for (size_t i = 0; i < _thread_count; i++) {
            if (_loopThreads[i]) _loopThreads[i]->Stop();
        }
    }

    Reactor *ThreadPool::NextReactor() {
        if (_thread_count == 0) {
            return _masterreactor;
        }
        Reactor *nextreactor = _slaverreactors[_next_idx.fetch_add(1) % _thread_count];
        return nextreactor;
    }

}
