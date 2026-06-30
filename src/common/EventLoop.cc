#include "EventLoop.h"
#include <stdexcept>
#include <condition_variable>


namespace NetWork{

    EventLoop::EventLoop()
    :_epoller(),
    _thread_id(std::this_thread::get_id()),  
    _timewheel(this),
    _signal_handler(this),
    _event_fd(-1),
    _channel(this, -1),
    _tasks(),
    _mutex(),
    _quit(false)
    {
        _event_fd = eventfd(0, EFD_NONBLOCK);
        if(_event_fd < 0){
            LOG(ERROR) << "eventfd create failed: " << std::strerror(errno) << " (errno: " << errno << ")";
            throw std::runtime_error("eventfd create failed");
        }
        _channel.SetFd(_event_fd);
        _channel.SetReadCallback([this](){ OnRead_eventfd(); });
        _channel.EnableRead();
    }

    EventLoop::~EventLoop(){
        QuitAndWait();  // 退出并等待 loop() 结束
        // loop 已停止, 直接移除 eventfd channel (不走 Channel::Remove, 其 assert isInLoopThread)
        _epoller.remove(&_channel);
        if(_event_fd >= 0){
            close(_event_fd);
        }
    }

    void EventLoop::OnRead_eventfd(){
        //读取eventfd中的数据，消费事件
        uint64_t count = 0;
        ssize_t ret = read(_event_fd, &count, sizeof(count));
        if(ret < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR){
                return;
            }
            LOG(ERROR) << "eventfd read failed: " << std::strerror(errno) << " (errno: " << errno << ")";
        }
    }

    void EventLoop::loop() {
        _thread_id = std::this_thread::get_id();  // 修正为实际运行 loop 的线程 ID
        _looping.store(true, std::memory_order_release);  // 标记 loop 已启动
        _timewheel.run();
        while(!_quit){
            vector<Channel*> actives;
            //1.事件监控
            _epoller.poll(actives);
            //2.处理事件
            for(auto channel: actives){
                try {
                    channel->HandleEvent();
                } catch (const std::exception& e) {
                    LOG(ERROR) << "Exception in channel handler: " << e.what();
                } catch (...) {
                    LOG(ERROR) << "Unknown exception in channel handler";
                }
            }
            //3.执行任务池中的所有任务
            RunAllTasks();
        }
        _looping.store(false, std::memory_order_release);  // loop 已退出
        // 通知 QuitAndWait 的调用者
        _quit_done.store(true, std::memory_order_release);
        _quit_cv.notify_all();
    }

    void EventLoop::WakeUpEvent(){
        //向eventfd写入数据，唤醒epoll_wait
        uint64_t count = 1;
        while (true) {
            ssize_t ret = write(_event_fd, &count, sizeof(count));
            if (ret < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return;
                }
                if (errno == EINTR) {
                    // 被信号中断，重试
                    continue;
                }
                LOG(ERROR) << "eventfd write failed: " << std::strerror(errno) << " (errno: " << errno << ")";
                return;
            }
            break;  // 写入成功
        }
    }

    void EventLoop::queueTask(LoopTask task){
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _tasks.push_back(task);
        }
        WakeUpEvent();
    }

    void EventLoop::runInLoop(LoopTask task){
        // loop 未启动时一律 queueTask, 避免 loop 启动前误判为同线程
        if(_looping.load(std::memory_order_acquire) && std::this_thread::get_id() == _thread_id){
            //当前线程内(说明上层直接使用connection)->直接执行任务
            task();
        }else{
            //其他线程(上层通过其它线程对connection使用)->放入任务队列
            queueTask(task);
        }
    }

    void EventLoop::RunAllTasks(){
        //从任务队列中取出所有任务,加锁，避免直接执行_tasks的任务的同时其它线程放入任务
        vector<LoopTask> tasks;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            tasks.swap(_tasks);
        }
        for(auto& task: tasks){
            try {
                task();
            } catch (const std::exception& e) {
                LOG(ERROR) << "Exception in task: " << e.what();
            } catch (...) {
                LOG(ERROR) << "Unknown exception in task";
            }
        }
    }

    void EventLoop::Quit(){
        _quit = true;
        WakeUpEvent();
    }

    void EventLoop::QuitAndWait(){
        Quit();
        if (!isInLoopThread()) {
            // loop 未启动则无需等待
            if (!_looping.load(std::memory_order_acquire)) return;
            std::unique_lock<std::mutex> lock(_quit_mtx);
            _quit_cv.wait(lock, [this] { return _quit_done.load(std::memory_order_acquire); });
        }
    }

    // 负载信息获取接口实现
    size_t EventLoop::GetChannelCount() const {
        // 返回 Epoller 中监听的 Channel 数量
        return _epoller.ChannelCount();
    }

    size_t EventLoop::GetTaskQueueSize() const {
        // 返回任务队列的大小
        std::lock_guard<std::mutex> lock(_mutex);
        return _tasks.size();
    }

    size_t EventLoop::GetLoad() const {
        // 默认负载算法：Channel 数量 * 10 + 任务队列大小
        // 可以根据实际需求调整权重
        return GetChannelCount() * 10 + GetTaskQueueSize();
    }


}
