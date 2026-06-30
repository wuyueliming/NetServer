#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>
#include "EventLoop.h"
#include "noncopyable.hpp"


namespace NetWork{

    class LoopThread : public noncopyable{
    public:
        LoopThread(const std::string& name = "");
        ~LoopThread();
        EventLoop *getLoop();/*返回当前线程关联的 EventLoop 对象指针*/
        void Stop();/*停止线程*/
    private:
        void Entry();/*实例化 EventLoop 对象，唤醒_cond上有可能阻塞的线程，并且开始运行 EventLoop 模块的功能*/

    private:
        std::string _name;                     // 线程名称
        std::mutex _mutex;          // 互斥锁
        std::condition_variable _cond;   // 条件变量，用于实现 _loop 获取的同步关系
        EventLoop* _loop;          // EventLoop 指针，对象在线程内实例化
        std::thread _thread;        // 线程
    };

}
