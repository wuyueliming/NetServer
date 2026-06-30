#pragma once

#include<unistd.h>
#include<sys/epoll.h>
#include<unordered_map>
#include<vector>
#include<cassert>
#include<cstring>
#include<cerrno>
#include<stdexcept>
#include"Logger.hpp"
#include"noncopyable.hpp"
#include"Channel.h"


namespace NetWork
{
    using std::unordered_map;
    using std::vector;
    static const int kInitEventListSize = 16;
    static const int kPollTimeMs = 10000;  // 10秒超时兜底，防止 eventfd 失败时永久阻塞

    class Epoller : public noncopyable
    {
    private:
        bool HasChannel(int fd){
            return _channels.find(fd) != _channels.end();
        }

        void control(Channel* channel, int op){
            epoll_event event;
            event.data.ptr = channel;
            event.events = channel->events();
            int ret = ::epoll_ctl(_epfd, op, channel->fd(), &event);
            if(ret < 0){
                if (op == EPOLL_CTL_DEL) {
                    // EPOLL_CTL_DEL 失败通常是因为 fd 已关闭，只记录日志不抛异常
                    LOG(ERROR) << "epoll_ctl DEL failed on fd " << channel->fd() << ": " << std::strerror(errno);
                } else {
                    LOG(ERROR) << "epoll_ctl failed on fd " << channel->fd() << " (op: " << op << "): " << std::strerror(errno) << " (errno: " << errno << ")";
                }
            }
        }

    public:
        Epoller():_evs(kInitEventListSize){
            _epfd = ::epoll_create(1);
            if(_epfd == -1){
                LOG(ERROR) << "epoll_create failed: " << std::strerror(errno) << " (errno: " << errno << ")";
                throw std::runtime_error("epoll_create failed");
            }
        }
        ~Epoller(){
            ::close(_epfd);
        }


        void update(Channel *channel){
            if(!HasChannel(channel->fd())){
                _channels.insert(std::make_pair(channel->fd(), channel));
                control(channel, EPOLL_CTL_ADD);
            }else{
                control(channel, EPOLL_CTL_MOD);
            }
        }

        void remove(Channel *channel){
            if(!HasChannel(channel->fd())){
                // Channel 不在 epoll 中，直接返回（可能未 start 就 stop，或已移除）
                return;
            }
            _channels.erase(channel->fd());
            control(channel, EPOLL_CTL_DEL);
        }

        //阻塞等待，带超时兜底
        void  poll(vector<Channel*> & Actives){
            int ret = ::epoll_wait(_epfd, _evs.data(), static_cast<int>(_evs.size()), kPollTimeMs);
            if(ret < 0){
                if(errno == EINTR){
                    return ;
                }
                LOG(ERROR) << "epoll_wait failed: " << std::strerror(errno) << " (errno: " << errno << ")";
                return;  // 非 EINTR 错误时 log 并返回，不 abort
            }
            // 动态扩容：活跃事件数等于数组大小时翻倍
            if (static_cast<size_t>(ret) == _evs.size()) {
                _evs.resize(_evs.size() * 2);
            }
            Actives.clear();
            for(int i = 0; i < ret; ++i){
                Channel* ch = static_cast<Channel*>(_evs[i].data.ptr);
                ch->set_revents(_evs[i].events);
                Actives.push_back(ch);
            }
        }

        // 获取当前监听的 Channel 数量（用于负载均衡）
        size_t ChannelCount() const {
            return _channels.size();
        }



    private:
        int _epfd;
        vector<struct epoll_event> _evs;
        unordered_map<int, Channel *> _channels;
    };


}
