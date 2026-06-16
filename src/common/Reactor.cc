#include "Reactor.h"
#include <stdexcept>
#include <condition_variable>


namespace Aether{

    Reactor::Reactor()
    :_epoller(),
    _timewheel(this),
    _signal_handler(this),
    _thread_id(std::this_thread::get_id()),
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

    Reactor::~Reactor(){
        Quit();
        _channel.Remove();
        if(_event_fd >= 0){
            close(_event_fd);
        }
    }

    void Reactor::OnRead_eventfd(){
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

    void Reactor::loop(){
        _timewheel.run();
        while(!_quit){
            vector<Channel*> actives;
            //1.事件监控
            _epoller.poll(actives);
            //2.处理事件
            for(auto channel: actives){
                channel->HandleEvent();
            }
            //3.执行任务池中的所有任务
            RunAllTasks();
        }
        // 通知 QuitAndWait 的调用者
        _quit_done.store(true, std::memory_order_release);
        _quit_cv.notify_all();
    }

    void Reactor::WakeUpEvent(){
        //向eventfd写入数据，唤醒epoll_wait
        uint64_t count = 1;
        ssize_t ret = write(_event_fd, &count, sizeof(count));
        if(ret < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                return;
            }
            LOG(ERROR) << "eventfd write failed: " << std::strerror(errno) << " (errno: " << errno << ")";
        }
    }

    void Reactor::queueTask(LoopTask task){
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _tasks.push_back(task);
        }
        WakeUpEvent();
    }

    void Reactor::runInLoop(LoopTask task){
        if(std::this_thread::get_id() == _thread_id){
            //当前线程内(说明上层直接使用connection)->直接执行任务
            task();
        }else{
            //其他线程(上层通过其它线程对connection使用)->放入任务队列
            queueTask(task);
        }
    }

    void Reactor::RunAllTasks(){
        //从任务队列中取出所有任务,加锁，避免直接执行_tasks的任务的同时其它线程放入任务
        vector<LoopTask> tasks;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            tasks.swap(_tasks);
        }
        for(auto& task: tasks){
            task();
        }
    }

    void Reactor::Quit(){
        _quit = true;
        WakeUpEvent();
    }

    void Reactor::QuitAndWait(){
        Quit();
        if (!isInLoopThread()) {
            std::unique_lock<std::mutex> lock(_quit_mtx);
            _quit_cv.wait(lock, [this] { return _quit_done.load(std::memory_order_acquire); });
        }
    }


}
