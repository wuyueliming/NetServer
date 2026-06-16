#pragma once

#include <cassert>
#include <cstdint>
#include <sys/types.h>
#include<vector>
#include<unordered_map>
#include<functional>
#include<memory>
#include "base/Logger.hpp"
#include"base/Timer.hpp"
#include"base/noncopyable.hpp"
#include"Channel.h"

class Reactor;


namespace Aether
{

    static constexpr int MAX_SECONDS = 60;
    static constexpr int MAX_MINUTES = 60;
    static constexpr int MAX_TIMEOUT = MAX_SECONDS * MAX_MINUTES - 1;  // 最大支持 3599 秒

    using  TaskID=uint64_t;
    using  TimedTask= std::function<void()> ;
    using  ReleaseCallback= std::function<void()> ;

    // TaskID 特殊值定义
    // 0: 无效 ID（保留，暂未使用）
    // 负数（转换为 TaskID 后）：设置任务出错
    // >= 1: 有效任务 ID
    static constexpr TaskID INVALID_TASK_ID = 0;
    static constexpr TaskID ERROR_TASK_ID = (TaskID)-1;

    class TimerTask
    {
    public:
        TimerTask(TaskID id,TimedTask function,uint32_t timeout, uint32_t fire_sec)
            : _taskid(id),_function(function),_timeout(timeout),_fire_sec(fire_sec),_canceled(false){}
        ~TimerTask(){
            if(_canceled==false){
                _function();
            }
            _releaseCallback();
        }

        void SetReleaseCallback(ReleaseCallback cb){
            _releaseCallback=cb;
        }
        void cancel(){
            _canceled=true;
        }
        bool IsCanceled() const {
            return _canceled;
        }
        uint32_t GetTimeout()const{
            return _timeout;
        }
        void SetTimeout(uint32_t timeout){
            _timeout=timeout;
        }
        uint32_t GetFireSec() const {
            return _fire_sec;
        }
        void SetFireSec(uint32_t fire_sec){
            _fire_sec = fire_sec;
        }
        const TimedTask& GetFunction() const {
            return _function;
        }

    private:
        TaskID _taskid;
        TimedTask _function;
        uint32_t _timeout;
        uint32_t _fire_sec;     // 降级时在秒轮中的目标位置
        bool  _canceled;
        ReleaseCallback _releaseCallback;
    };



        using WeakTimerTaskPtr = std::weak_ptr<TimerTask>;
        using SharedTimerTaskPtr = std::shared_ptr<TimerTask>;
    class TimeWheel : public noncopyable
    {
    public:
        TimeWheel(Reactor* loop);
        ~TimeWheel();

        void run();

        /*定时器中有个_timers成员，定时器信息的操作有可能在多线程中进行，因此需要考虑线程安全问题*/
        /*如果不想加锁，那就把对定期的所有操作，都放到一个线程中进行*/
        void AddTask(uint32_t timeout,TimedTask function,TaskID* out_id);
        void RefreshTask(TaskID id,uint32_t timeout);
        void CancelTask(TaskID id);

        /*这个接口存在线程安全问题--这个接口实际上不能被外界使用者调用，只能在模块内，在对应的EventLoop线程内执行*/
        bool HasTimer(uint64_t id);//weak thread safe

    private:
        void InitTimer();
        void tick();
        void OnRead_timer();
        void RemoveTask(TaskID id);
        //InLoop
        void AddTaskInLoop(uint32_t timeout,TimedTask function,TaskID* out_id);
        void RefreshTaskInLoop(TaskID id,uint32_t timeout);
        void CancelTaskInLoop(TaskID id);

    private:
        Reactor *_loop;
        Timer _timer;
        Channel _channel;

        int _tick_sec;          // 秒针
        int _tick_min;          // 分针
        TaskID _nextid;
        std::vector<std::vector<SharedTimerTaskPtr>> _sec_wheel;   // 秒级时间轮（60 槽）
        std::vector<std::vector<SharedTimerTaskPtr>> _min_wheel;   // 分级时间轮（60 槽）
        std::unordered_map<TaskID,WeakTimerTaskPtr> _taskmap;
    };

}
