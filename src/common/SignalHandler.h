#pragma once

#include <signal.h>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "Channel.h"

namespace NetWork {

class EventLoop;

// SignalHandler：使用 signalfd 将信号转换为 fd 事件，集成到 EventLoop
class SignalHandler {
public:
    using SignalCallback = std::function<void()>;

    explicit SignalHandler(EventLoop *loop);
    ~SignalHandler();

    // 注册信号处理函数
    void Register(int signo, SignalCallback handler);

    // 在创建任何线程前调用，阻塞指定的信号
    // 确保信号只会被 signalfd 接收，不会被任意线程中断
    static void BlockSignals(const std::vector<int>& signals);

private:
    void OnRead();  // signalfd 可读回调

    EventLoop *_loop;
    int _sigfd;
    Channel _channel;
    sigset_t _mask;
    std::unordered_map<int, SignalCallback> _handlers;
    std::mutex _mutex;  // 保护 _handlers 和 _mask 的线程安全
};

}