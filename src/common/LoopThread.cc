#include "LoopThread.h"
#include <pthread.h>

namespace NetWork{

    LoopThread::LoopThread(const std::string& name)
    :_name(name), _loop(nullptr), _thread(&LoopThread::Entry, this) {}

    EventLoop *LoopThread::getLoop() {
        EventLoop *loop = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [&](){ return _loop != nullptr; });
            loop = _loop;
        }
        return loop;
    }

    void LoopThread::Entry() {
        // 设置线程名称（Linux 限制最多 16 字节，含结尾 \0）
        if (!_name.empty()) {
            pthread_setname_np(pthread_self(), _name.substr(0, 15).c_str());
        }
        EventLoop loop;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _loop = &loop;
            _cond.notify_one();
        }
        loop.loop();
    }

    void LoopThread::Stop() {
        EventLoop *loop = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [this](){ return _loop != nullptr; });
            loop = _loop;
        }
        if (loop) loop->Quit();
    }

    LoopThread::~LoopThread() {
        Stop();
        if (_thread.joinable()) _thread.join();
    }

}
