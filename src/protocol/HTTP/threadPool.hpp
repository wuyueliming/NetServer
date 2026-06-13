#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    ThreadPool(int count):_stop(false) {
        for (int i = 0; i < count; i++) {
            _workers.emplace_back([this](){ WorkerLoop(); });
        }
    }
    ~ThreadPool() {
        _stop = true;
        _cv.notify_all();
        for (auto &t : _workers) {
            if (t.joinable()) t.join();
        }
    }
    void Push(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _tasks.push(std::move(task));
        }
        _cv.notify_one();
    }
private:
    void WorkerLoop() {
        while (!_stop) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _cv.wait(lock, [this](){ return _stop || !_tasks.empty(); });
                if (_stop && _tasks.empty()) return;
                if (_tasks.empty()) continue;
                task = std::move(_tasks.front());
                _tasks.pop();
            }
            task();
        }
    }
private:
    std::vector<std::thread> _workers;
    std::queue<std::function<void()>> _tasks;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> _stop;
};