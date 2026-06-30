#pragma once

#include "Epoller.hpp"
#include "Channel.h"
#include "TimeWheel.h"
#include "SignalHandler.h"
#include "Logger.hpp"
#include "noncopyable.hpp"
#include<vector>
#include<functional>
#include<mutex>
#include<atomic>
#include<sys/eventfd.h>
#include<thread>
#include <unistd.h>
#include <cstring>

namespace NetWork{
    using std::vector;


    using LoopTask = std::function<void()>;
    class EventLoop : public noncopyable{
    public:
        EventLoop();
        ~EventLoop();
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
        //6.信号处理
        SignalHandler& signalHandler() { return _signal_handler; }
        EventLoop* getLoop() { return this; }
        //7.负载信息获取（用于负载均衡）
        size_t GetChannelCount() const;      // 获取当前监听的 Channel 数量
        size_t GetTaskQueueSize() const;     // 获取任务队列大小
        size_t GetLoad() const;              // 获取综合负载值（默认算法）
    private:
        void RunAllTasks();
        void WakeUpEvent();
        void OnRead_eventfd();
    private:
        //事件循环
        Epoller _epoller;
        //多线程：线程ID必须在其他依赖 EventLoop 的成员之前初始化
        std::thread::id _thread_id;//线程ID
        //定时任务
        TimeWheel _timewheel;
        //信号处理
        SignalHandler _signal_handler;
        int _event_fd;//eventfd唤醒IO事件监控有可能导致的阻塞
        Channel _channel;// eventfd唤醒
        vector<LoopTask> _tasks;//本线程任务队列
        mutable std::mutex _mutex;//任务队列互斥锁（mutable 允许 const 方法加锁）
        std::atomic<bool> _quit;//退出标志
        std::mutex _quit_mtx;               // QuitAndWait 等待用
        std::condition_variable _quit_cv;   // QuitAndWait 等待用
        std::atomic<bool> _quit_done{false};// loop 是否已退出
        std::atomic<bool> _looping{false};  // loop() 是否已启动运行
    };
}
