#pragma once

#include"LoopThread.h"
#include "Reactor.h"
#include "base/Logger.hpp"
#include "base/noncopyable.hpp"
#include <vector>
#include <memory>
#include <atomic>

namespace Aether{
    using std::vector;
    class ThreadPool : public noncopyable{
    public:
        ThreadPool(Reactor *masterloop);
        ~ThreadPool() = default;
        void SetThreadCount(int count);
        void Create();//创建线程池
        void Stop();//停止线程池
        Reactor *NextReactor();//获取下一个Reactor对象
        const vector<Reactor*>& GetAllReactors() const { return _slaverreactors; }

    private:
        size_t _thread_count;
        std::atomic<size_t> _next_idx;
        Reactor* _masterreactor;//主reactor指针
        vector<std::unique_ptr<LoopThread>> _loopThreads;//线程池
        vector<Reactor*> _slaverreactors;//从reactor指针
    };
}
