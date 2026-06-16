#pragma once

#include "base/Epoller.hpp"
#include "Channel.h"
#include "TimeWheel.h"
#include "SignalHandler.h"
#include "base/Logger.hpp"
#include "base/noncopyable.hpp"
#include<vector>
#include<functional>
#include<mutex>
#include<atomic>
#include<sys/eventfd.h>
#include<thread>
#include <unistd.h>
#include <cstring>

namespace Aether{
    using std::vector;


    using LoopTask = std::function<void()>;
    class Reactor : public noncopyable{
    public:
        Reactor();
        ~Reactor();
        //1.事件监听管理
        void RemoveEvent(Channel* channel){_epoller.remove(channel);}
        void UpdateEvent(Channel* channel){_epoller.update(channel);}
        //2.事件循环
        void loop();
        //3.定时任务管理
        bool existTask(TaskID id){return _timewheel.HasTimer(id);}
        void AddTimedTask(uint32_t timeout,TimedTask function,TaskID* out_id=nullptr){_timewheel.AddTask(timeout,function,out_id);}
        void RefreshTimedTask(TaskID id,uint32_t timeout){_timewheel.RefreshTask(id,timeout);}
        void CancelTimedTask(TaskID id){_timewheel.CancelTask(id);}
        //4.线程安全任务管理
        void runInLoop(LoopTask task);
        void queueTask(LoopTask task);
        bool isInLoopThread(){return std::this_thread::get_id() == _thread_id;}
        //5.退出事件循环
        void Quit();
        void QuitAndWait();  // 退出并等待循环结束（线程安全）
        void stop() { Quit(); }  // 别名，与上层调用方兼容
        //6.信号处理
        SignalHandler& signalHandler() { return _signal_handler; }
        Reactor* GetLoop() { return this; }  // 返回自身指针，与现有调用方兼容
    private:
        void RunAllTasks();
        void WakeUpEvent();
        void OnRead_eventfd();
    private:
        //事件循环
        Epoller _epoller;
        //定时任务
        TimeWheel _timewheel;
        //信号处理
        SignalHandler _signal_handler;
        //多线程
        std::thread::id _thread_id;//线程ID
        int _event_fd;//eventfd唤醒IO事件监控有可能导致的阻塞
        Channel _channel;// eventfd唤醒
        vector<LoopTask> _tasks;//本线程任务队列
        std::mutex _mutex;//任务队列互斥锁
        std::atomic<bool> _quit;//退出标志
        std::mutex _quit_mtx;               // QuitAndWait 等待用
        std::condition_variable _quit_cv;   // QuitAndWait 等待用
        std::atomic<bool> _quit_done{false};// loop 是否已退出
    };
}
