#include "LoopThreadPool.h"
#include <limits>

namespace NetWork{

    // DefaultLoadBalancer 实现：简单轮询
    size_t DefaultLoadBalancer::Select(const vector<EventLoop*>& loops) {
        if (loops.empty()) return 0;
        size_t idx = _next_idx.fetch_add(1) % loops.size();
        return idx;
    }

    LoopThreadPool::LoopThreadPool()
    :_thread_count(0), _name_prefix("IO") {
        // 创建默认负载均衡器
        _defaultBalancer = std::make_unique<DefaultLoadBalancer>();
    }

    void LoopThreadPool::SetThreadCount(int count) {
        _thread_count = count;
    }

    void LoopThreadPool::SetThreadNamePrefix(const std::string& prefix) {
        _name_prefix = prefix;
    }

    void LoopThreadPool::SetLoadBalancer(LoadBalanceCallback cb) {
        _loadBalancer = std::move(cb);
    }

    void LoopThreadPool::Create() {
        if (_thread_count > 0) {
            _loopThreads.resize(_thread_count);
            _slaverloops.resize(_thread_count);
            for (size_t i = 0; i < _thread_count; i++) {
                std::string name = _name_prefix + "-" + std::to_string(i);
                _loopThreads[i] = std::make_unique<LoopThread>(name);
                _slaverloops[i] = _loopThreads[i]->getLoop();
            }
        }
        LOG(INFO) << "LoopThreadPool Created, thread count:" << _thread_count;
        return;
    }

    void LoopThreadPool::Stop() {
        for (size_t i = 0; i < _thread_count; i++) {
            if (_loopThreads[i]) _loopThreads[i]->Stop();
        }
    }

    EventLoop *LoopThreadPool::NextLoop() {
        if (_thread_count == 0) {
            return nullptr;
        }

        // 使用负载均衡回调获取下标
        size_t idx = _loadBalancer ? _loadBalancer(_slaverloops)
                                   : _defaultBalancer->Select(_slaverloops);
        return _slaverloops[idx];
    }

}