#pragma once

#include"LoopThread.h"
#include "EventLoop.h"
#include "Logger.hpp"
#include "noncopyable.hpp"
#include <vector>
#include <memory>
#include <atomic>
#include <string>
#include <functional>

namespace NetWork{
    using std::vector;

    // 负载均衡回调：从 loop 数组中选择一个，返回下标
    using LoadBalanceCallback = std::function<size_t(const vector<EventLoop*>&)>;

    // 默认负载均衡器：Round-Robin + 负载感知
    class DefaultLoadBalancer {
    public:
        DefaultLoadBalancer() : _next_idx(0) {}

        // 选择策略：Round-Robin
        size_t Select(const vector<EventLoop*>& loops);

    private:
        std::atomic<size_t> _next_idx{0};
    };

    class LoopThreadPool : public noncopyable{
    public:
        LoopThreadPool();
        ~LoopThreadPool() = default;
        void SetThreadCount(int count);
        void SetThreadNamePrefix(const std::string& prefix);  // 设置线程名前缀
        void SetLoadBalancer(LoadBalanceCallback cb);         // 设置自定义负载均衡策略
        void Create();//创建线程池
        void Stop();//停止线程池
        EventLoop *NextLoop();//负载均衡：返回负载最小的EventLoop，线程数为0时返回nullptr
        const vector<EventLoop*>& GetAllLoops() const { return _slaverloops; }

    private:
        size_t _thread_count;
        std::string _name_prefix = "LoopThread";             // 线程名前缀
        LoadBalanceCallback _loadBalancer;                   // 用户自定义负载均衡策略
        std::unique_ptr<DefaultLoadBalancer> _defaultBalancer; // 内置负载均衡器
        vector<std::unique_ptr<LoopThread>> _loopThreads;//线程池
        vector<EventLoop*> _slaverloops;//从EventLoop指针
    };
}