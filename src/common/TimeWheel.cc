#include "TimeWheel.h"
#include "EventLoop.h"

namespace NetWork
{
    TimeWheel::TimeWheel(EventLoop* loop)
        : _loop(loop),_timer(),_channel(loop, -1),_tick_sec(0),_tick_min(0),_nextid(1),
          _sec_wheel(MAX_SECONDS),_min_wheel(MAX_MINUTES)
    {
        InitTimer();
        _channel.SetFd(_timer.fd());
        _channel.SetReadCallback([this](){ OnRead_timer(); });
    }

    TimeWheel::~TimeWheel(){
        _timer.close();
    }

    void TimeWheel::run(){
        _channel.EnableRead();
    }

    void TimeWheel::AddTask(uint32_t timeout,TimedTask function,TaskID* out_id){
        _loop->runInLoop([this, timeout, function = std::move(function), out_id](){
            AddTaskInLoop(timeout, function, out_id);
        });
    }
    void TimeWheel::RefreshTask(TaskID id,uint32_t timeout){
        _loop->runInLoop([this, id, timeout](){ RefreshTaskInLoop(id, timeout); });
    }
    void TimeWheel::CancelTask(TaskID id){
        _loop->runInLoop([this, id](){ CancelTaskInLoop(id); });
    }

    void TimeWheel::InitTimer(){
        _timer.create();
        struct itimerspec itime;
        itime.it_value.tv_sec = 1;
        itime.it_value.tv_nsec = 0;
        itime.it_interval.tv_sec = 1;
        itime.it_interval.tv_nsec = 0;
        _timer.settime(itime);
    }

    void TimeWheel::tick(){
        // 秒针前进
        _tick_sec = (_tick_sec + 1) % MAX_SECONDS;

        // 每当秒针转满一圈（回到0），分针前进一格，先降级再清空
        if(_tick_sec == 0){
            _tick_min = (_tick_min + 1) % MAX_MINUTES;
            // 将分针当前槽位的所有任务降级到秒级时间轮
            auto& min_slot = _min_wheel[_tick_min];
            for(auto& task : min_slot){
                if(task && !task->IsCanceled()){
                    // 使用存储的 fire_sec 定位秒轮槽位
                    _sec_wheel[task->GetFireSec()].push_back(task);
                }
            }
            min_slot.clear();
        }

        // 清空当前秒槽（执行到期任务）
        _sec_wheel[_tick_sec].clear();
    }

    void TimeWheel::OnRead_timer(){
        ssize_t count = _timer.read();
        if (count < 0) {
            LOG(ERROR) << "timerfd read failed";
            return;
        }
        for(int i = 0; i < count; i++){
            tick();
        }
    }

    void TimeWheel::RemoveTask(TaskID id){
        _taskmap.erase(id);
    }




    void TimeWheel::AddTaskInLoop(uint32_t timeout,TimedTask function,TaskID* out_id){
        TaskID id = _nextid++;
        if(timeout == 0){
            function();
            if(out_id != nullptr){
                *out_id = 0;  // 0 表示无效 id（立即执行的任务不支持取消）
            }
            return;
        }

        // 限制最大超时，防止 total_minutes % MAX_MINUTES 导致长超时提前触发
        if(timeout > (uint32_t)MAX_TIMEOUT){
            LOG(ERROR) << "Timeout " << timeout << "s exceeds max " << MAX_TIMEOUT << "s, clamped";
            timeout = MAX_TIMEOUT;
        }

        // 使用绝对时间计算目标位置，避免 _tick_sec 偏移导致的精度问题
        uint32_t absolute_fire = _tick_min * MAX_SECONDS + _tick_sec + timeout;
        uint32_t fire_sec = absolute_fire % MAX_SECONDS;
        uint32_t total_minutes = absolute_fire / MAX_SECONDS;

        SharedTimerTaskPtr task = std::make_shared<TimerTask>(id, function, timeout, fire_sec);
        task->SetReleaseCallback([this, id](){ RemoveTask(id); });
        _taskmap[id] = task;

        if(total_minutes == (uint32_t)_tick_min || timeout < MAX_SECONDS){
            // 放入秒级时间轮（当前分钟内或 timeout < 60）
            _sec_wheel[fire_sec].push_back(task);
        } else {
            // 放入分级时间轮
            int min_pos = total_minutes % MAX_MINUTES;
            _min_wheel[min_pos].push_back(task);
        }

        if(out_id != nullptr){
            *out_id = id;
        }
    }

    void TimeWheel::RefreshTaskInLoop(TaskID id,uint32_t timeout){
        auto it = _taskmap.find(id);
        if(it == _taskmap.end()){
            return;
        }
        auto ptask = it->second.lock();
        if(!ptask){
            _taskmap.erase(it);
            return;
        }

        // 刷新逻辑: 直接在新的位置上插入同一个 shared_ptr
        // 旧槽位 tick 到期清空时 refcount-- 但不归零 (新槽位还持有副本),
        // 因此旧任务析构不会触发, 只有新槽位到期才触发 function。
        // _taskmap 无需改动 (仍指向同一个 task)。
        if(timeout > (uint32_t)MAX_TIMEOUT){
            LOG(ERROR) << "Timeout " << timeout << "s exceeds max " << MAX_TIMEOUT << "s, clamped";
            timeout = MAX_TIMEOUT;
        }

        uint32_t absolute_fire = _tick_min * MAX_SECONDS + _tick_sec + timeout;
        uint32_t fire_sec = absolute_fire % MAX_SECONDS;
        uint32_t total_minutes = absolute_fire / MAX_SECONDS;

        // 更新 fire_sec 与 timeout, 供分针降级时定位秒轮槽位使用
        ptask->SetFireSec(fire_sec);
        ptask->SetTimeout(timeout);

        if(total_minutes == (uint32_t)_tick_min || timeout < MAX_SECONDS){
            _sec_wheel[fire_sec].push_back(ptask);
        } else {
            int min_pos = total_minutes % MAX_MINUTES;
            _min_wheel[min_pos].push_back(ptask);
        }
    }

    void TimeWheel::CancelTaskInLoop(TaskID id){
        auto it = _taskmap.find(id);
        if(it == _taskmap.end()){
            return;
        }
        auto ptask = it->second.lock();
        if(ptask){
            ptask->cancel();
        }
        // 立即清理 _taskmap，避免积累已取消的 expired weak_ptr
        _taskmap.erase(it);
    }

    bool TimeWheel::HasTimer(uint64_t id) {
        // 必须在 EventLoop 线程调用，否则需要通过 runInLoop 串行化
        assert(_loop->isInLoopThread());
        auto it = _taskmap.find(id);
        if (it == _taskmap.end()) {
            return false;
        }
        return true;
    }

}
