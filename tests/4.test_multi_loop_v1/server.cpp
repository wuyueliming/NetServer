#include "../../src/common/InetAddr.hpp"
#include "../../src/common/Logger.hpp"
#include "../../src/server/TcpServer.h"
#include "../../src/common/Connection.h"
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <deque>
#include <atomic>

using namespace NetWork;

//线程池：模拟业务线程池，处理接收到的数据
class BusinessPool {
public:
    BusinessPool(int count):_stop(false) {
        for (int i = 0; i < count; i++) {
            _workers.emplace_back([this](){ WorkerLoop(); });
        }
    }
    ~BusinessPool() {
        _stop = true;
        _cv.notify_all();
        for (auto &t : _workers) {
            if (t.joinable()) t.join();
        }
    }
    void Push(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _tasks.push_back(std::move(task));
        }
        _cv.notify_one();
    }
private:
    void WorkerLoop() {
        while (!_stop) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _cv.wait_for(lock, std::chrono::milliseconds(100), [this](){ return _stop || !_tasks.empty(); });
                if (_stop && _tasks.empty()) return;
                if (_tasks.empty()) continue;
                task = std::move(_tasks.front());
                _tasks.pop_front();
            }
            try {
                task();
            } catch (const std::exception &e) {
                LOG(ERROR) << "BusinessPool task exception: " << e.what();
            }
        }
    }
private:
    std::vector<std::thread> _workers;
    std::deque<std::function<void()>> _tasks;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> _stop;
};

//全局对象
TcpServer *g_server = nullptr;
BusinessPool *g_pool = nullptr;
std::atomic<int> g_conn_count{0};
std::atomic<int> g_msg_count{0};

//1.连接建立回调
void OnConnected(ConnectionPtr conn) {
    g_conn_count++;
    LOG(INFO) << "New connection: " << conn->PeerAddr().StrAddr()
                        << " connID:" << conn->ID()
                        << " total:" << g_conn_count.load();
}

//2.消息回调：将业务处理丢到线程池中
void OnMessage(ConnectionPtr conn) {
    Buffer* buf = conn->ReadBuffer();
    if (buf->ReadAbleSize() > 0) {
        std::string str = buf->ReadAsStringAndPop(buf->ReadAbleSize());
        g_msg_count++;
        //将回显业务丢到线程池中处理，模拟耗时业务
        g_pool->Push([conn, str = std::move(str)](){
            //在线程池线程中调用Send，Send内部会通过runInLoop保证线程安全
            conn->Send(str.c_str(), str.size());
        });
    }
}

//3.连接关闭回调
void OnClosed(ConnectionPtr conn) {
    g_conn_count--;
    LOG(INFO) << "Connection closed: " << conn->PeerAddr().StrAddr()
                        << " connID:" << conn->ID()
                        << " total:" << g_conn_count.load();
}

//4.任意事件回调
void OnEvent(ConnectionPtr conn) {
    //可以用来刷新连接的活跃度等
}

//5.定时心跳任务
void HeartbeatTask() {
    static int count = 0;
    LOG(INFO) << "Server heartbeat: " << ++count
                        << " connections:" << g_conn_count.load()
                        << " messages:" << g_msg_count.load();
    if (g_server) {
        g_server->AddTimedTask(HeartbeatTask, 3);
    }
}

//6.定时统计任务
void StatsTask() {
    static int last_msg = 0;
    int curr = g_msg_count.load();
    LOG(INFO) << "Stats: " << (curr - last_msg) << " msg/s, total:" << curr;
    last_msg = curr;
    if (g_server) {
        g_server->AddTimedTask(StatsTask, 5);
    }
}

int main() {
    const int port = 8080;

    //创建业务线程池
    BusinessPool pool(4);
    g_pool = &pool;

    //创建服务器
    NetWork::EventLoop loop;
    TcpServer server(&loop, port);
    g_server = &server;

    //设置从 EventLoop 线程数
    server.setThreadNum(3);

    //设置超时销毁：30秒无活动则断开
    server.enableInactiveRelease(30);

    //注册回调
    server.setConnectionCallback(OnConnected);
    server.setMessageCallback(OnMessage);
    server.setCloseCallback(OnClosed);
    server.setEventCallback(OnEvent);

    //注册定时任务
    server.AddTimedTask(HeartbeatTask, 3);
    server.AddTimedTask(StatsTask, 5);

    LOG(INFO) << "Echo Server starting on port " << port
                        << " with 3 sub-reactors and 4 business threads";

    server.start();  // 仅开始监听，不创建线程
    loop.loop();     // 用户自行调用，阻塞在此

    return 0;
}
