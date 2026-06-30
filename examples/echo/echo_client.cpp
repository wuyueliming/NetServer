// echo_client.cpp - 规范用法: EventLoop + connect/stop
//
// 用户自行创建线程跑 loop, 用 cv + connectionCallback 同步等待连接建立
// (如需异步连接: 去掉 cv 等待, 立即返回即可)
//
// 用法:
//   ./echo_client
//   输入文本回车, 服务端会回显; 输入 quit 退出

#include "EventLoop.h"
#include "TcpClient.h"
#include "Connection.h"
#include "Logger.hpp"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

using namespace NetWork;

static std::mutex g_mtx;
static std::condition_variable g_cv;
static bool g_connected = false;

static void OnMessage(ConnectionPtr conn) {
    Buffer* buf = conn->ReadBuffer();
    if (buf->ReadAbleSize() > 0) {
        std::string echo = buf->ReadAsStringAndPop(buf->ReadAbleSize());
        std::cout << "[echo] " << echo << std::endl;
    }
}

int main() {
    NetWork::EventLoop loop;
    TcpClient client(&loop, InetAddr(8080, "127.0.0.1"), "EchoClient");

    client.setMessageCallback(OnMessage);
    client.setConnectionCallback([](ConnectionPtr conn) {
        if (conn->isConnected()) {
            std::cout << "[callback] connected to " << conn->PeerAddr().StrAddr() << std::endl;
            { std::lock_guard<std::mutex> lk(g_mtx); g_connected = true; }
            g_cv.notify_one();
        } else {
            std::cout << "[callback] disconnected" << std::endl;
            { std::lock_guard<std::mutex> lk(g_mtx); g_connected = false; }
            g_cv.notify_one();
        }
    });

    // 1. 用户自行创建线程跑 loop (connect 的事件回调都在该线程触发)
    std::thread loop_thread([&loop] { loop.loop(); });

    // 2. 发起连接 (线程安全, 内部 runInLoop 投递到 loop 线程)
    client.connect();

    // 3. 同步等待连接建立 (3s 超时)
    {
        std::unique_lock<std::mutex> lk(g_mtx);
        g_cv.wait_for(lk, std::chrono::seconds(3), [] { return g_connected; });
    }
    if (!g_connected) {
        std::cerr << "connect failed or timeout" << std::endl;
        client.stop();
        loop_thread.join();
        return -1;
    }
    std::cout << "connected! type message (or 'quit'):" << std::endl;

    // 4. 主线程读 stdin, 通过 connection() 发送
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") break;
        auto conn = client.connection();
        if (conn && conn->isConnected()) {
            conn->Send(line.data(), line.size());
        } else {
            std::cerr << "connection lost" << std::endl;
            break;
        }
    }

    // 5. 停止: client.stop() 退出 loop, 用户 join 自己的 loop 线程
    client.stop();
    loop_thread.join();
    return 0;
}
