#pragma once

#include <sys/epoll.h>
#include <functional>
#include <memory>
#include "base/noncopyable.hpp"


namespace Aether{

    class Reactor;

    //管理socket的监听事件
    class Channel : public noncopyable{
    public:
        using EventCallback = std::function<void()>;

        Channel(Reactor *loop, int fd):_fd(fd), _loop(loop), _events(0), _revents(0), _tied(false) {}
        int fd() const { return _fd; }
        void SetFd(int fd) { _fd = fd; }
        Reactor* GetLoop() const { return _loop; }
        void SetLoop(Reactor *loop) { _loop = loop; }
        uint32_t events() const { return _events; }//获取想要监控的事件
        void set_revents(uint32_t events) { _revents = events; }//设置实际就绪的事件
        void SetReadCallback(const EventCallback &cb) { _read_callback = cb; }
        void SetWriteCallback(const EventCallback &cb) { _write_callback = cb; }
        void SetErrorCallback(const EventCallback &cb) { _error_callback = cb; }
        void SetCloseCallback(const EventCallback &cb) { _close_callback = cb; }
        void SetEventCallback(const EventCallback &cb) { _event_callback = cb; }
        //绑定对象生命周期：防止事件处理过程中对象被销毁
        void Tie(const std::shared_ptr<void>& obj) {
            _tie = obj;
            _tied = true;
        }
        //当前是否监控了可读
        bool ReadAble() const { return (_events & EPOLLIN); }
        //当前是否监控了可写
        bool WriteAble() const { return (_events & EPOLLOUT); }
        //启动读事件监控
        void EnableRead() { _events |= EPOLLIN; Update(); }
        //启动读事件监控(ET模式)，同时监控 EPOLLRDHUP 以检测对端关闭
        void EnableReadET() { _events |= EPOLLIN | EPOLLET | EPOLLRDHUP; Update(); }
        //启动写事件监控
        void EnableWrite() { _events |= EPOLLOUT; Update(); }
        //启动写事件监控(ET模式)
        void EnableWriteET() { _events |= EPOLLOUT | EPOLLET; Update(); }
        //关闭读事件监控
        void DisableRead() { _events &= ~EPOLLIN; Update(); }
        //关闭写事件监控
        void DisableWrite() { _events &= ~EPOLLOUT; Update(); }
        //关闭所有事件监控
        void DisableAll() { _events = 0; Update(); }
        //移除监控
        void Remove();
        void Update();
        //事件处理，一旦连接触发了事件，就调用这个函数，自己触发了什么事件如何处理自己决定
        void HandleEvent() {
            // 如果绑定了对象生命周期，先检查对象是否存活
            std::shared_ptr<void> guard;
            if (_tied) {
                guard = _tie.lock();
                if (!guard) return; // 对象已销毁，不再处理事件
            }

            // EPOLLRDHUP 表示对端关闭连接或关闭写端，应触发 close 回调
            if (_revents & EPOLLRDHUP) {
                if (_close_callback) _close_callback();
            }
            // EPOLLIN 独立检查，因为 EPOLLRDHUP 和 EPOLLIN 可能同时触发
            // 对端发送数据后关闭写端时，必须先读取剩余数据再关闭连接
            if ((_revents & EPOLLIN) || (_revents & EPOLLPRI)) {
                if (_read_callback) _read_callback();
            }
            // EPOLLOUT、EPOLLERR、EPOLLHUP 独立检查，避免互相遮蔽
            if (_revents & EPOLLOUT) {
                if (_write_callback) _write_callback();
            }
            if (_revents & EPOLLERR) {
                if (_error_callback) _error_callback();
            }
            if ((_revents & EPOLLHUP) && !(_revents & EPOLLRDHUP)) {
                // EPOLLHUP 但非 EPOLLRDHUP 的情况（如管道/套接字异常关闭）
                if (_close_callback) _close_callback();
            }
            if (_event_callback) _event_callback();
            _revents = 0;
        }

    private:
        int _fd;//对任意fd的监控
        Reactor* _loop;
        uint32_t _events;
        uint32_t _revents;
        EventCallback _read_callback;   //可读事件被触发的回调函数
        EventCallback _write_callback;  //可写事件被触发的回调函数
        EventCallback _error_callback;  //错误事件被触发的回调函数
        EventCallback _close_callback;  //连接断开事件被触发的回调函数
        EventCallback _event_callback;  //任意事件被触发的回调函数
        std::weak_ptr<void> _tie;       //绑定对象的生命周期
        bool _tied;                     //是否绑定了对象
    };


}
