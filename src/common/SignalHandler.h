#pragma once

#include <signal.h>
#include <functional>
#include <unordered_map>
#include <vector>
#include "Channel.h"

namespace Aether {

class Reactor;

// SignalHandler：使用 signalfd 将信号转换为 fd 事件，集成到 Reactor
// 设计理念（参考 Boost.Asio signal_set）：
// 1. 将信号转换为 fd 事件，统一纳入 epoll 监控
// 2. 避免信号处理函数的异步安全问题
// 3. 支持在 Reactor 线程中安全地处理信号
class SignalHandler {
public:
    using SignalCallback = std::function<void()>;

    explicit SignalHandler(Reactor *loop);
    ~SignalHandler();

    // 注册信号处理函数
    void Register(int signo, SignalCallback handler);

    // 在创建任何线程前调用，阻塞指定的信号
    // 确保信号只会被 signalfd 接收，不会被任意线程中断
    static void BlockSignals(const std::vector<int>& signals);

private:
    void OnRead();  // signalfd 可读回调

    Reactor *_loop;
    int _sigfd;
    Channel _channel;
    sigset_t _mask;
    std::unordered_map<int, SignalCallback> _handlers;
};

}