#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include "Reactor.h"
#include "base/noncopyable.hpp"


namespace Aether{

    class LoopThread : public noncopyable{
    public:
        LoopThread();/*创建线程，设定线程入口函数*/
        ~LoopThread();
        Reactor *GetReactor();/*返回当前线程关联的 Reactor 对象指针*/
        Reactor *GetLoop();/*返回 Reactor 内部的 Reactor 指针*/
        void Stop();/*停止线程*/
    private:
        void Entry();/*实例化 Reactor 对象，唤醒_cond上有可能阻塞的线程，并且开始运行 Reactor 模块的功能*/

    private:
        std::mutex _mutex;          // 互斥锁
        std::condition_variable _cond;   // 条件变量，用于实现 _reactor 获取的同步关系
        Reactor *_reactor;          // Reactor 指针，对象在线程内实例化
        std::thread _thread;        // 线程
    };

}
