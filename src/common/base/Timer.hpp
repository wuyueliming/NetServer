#pragma once

#include <unistd.h>
#include <sys/timerfd.h>
#include "Logger.hpp"

namespace Aether
{

    class Timer
    {
    public:
        Timer():_timerfd(-1){}
        Timer(int fd):_timerfd(fd){}
        ~Timer(){
            if(_timerfd >= 0) ::close(_timerfd);
        }

    public:
        bool create()
        {
            _timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
            if (_timerfd < 0)
            {
                LOG(ERROR) << "timerfd_create error";
                return false;
            }
            return true;
        }
        bool close(){
            if(_timerfd < 0) return true;
            int ret = ::close(_timerfd);
            _timerfd = -1;
            if(ret<0){
                LOG(ERROR) << "timerfd_close error";
                return false;
            }
            return true;
        }

        ssize_t read(){
            uint64_t count;
            int ret = ::read(_timerfd, &count, sizeof(count));
            if(ret<0){
                LOG(ERROR) << "timerfd_read error";
                return -1;
            }
            return count;
        }

        void settime(const struct itimerspec &new_value)
        {
            if (timerfd_settime(_timerfd, 0, &new_value, nullptr) < 0)
            {
                LOG(ERROR) << "timer set error";
            }
        }

        void gettime(struct itimerspec *curr_value)
        {
            if (timerfd_gettime(_timerfd, curr_value) < 0)
            {
                LOG(ERROR) << "timer get error";
            }
        }

        int fd() const { return _timerfd; }

    private:
        int _timerfd;
    };
}
