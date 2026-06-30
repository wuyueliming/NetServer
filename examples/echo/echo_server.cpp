// echo_server.cpp - 规范用法: EventLoop + start/stop
//
// 阻塞模式 (本文件): server.start() + loop.loop() 在主线程
// 后台模式 (变体):   std::thread t([&loop]{ loop.loop(); }); server.start();
//                    主线程不阻塞, 退出前 server.stop(); t.join();
//
// 用法:
//   ./echo_server
//   连接: echo_client

#include "EventLoop.h"
#include "TcpServer.h"
#include "Connection.h"
#include "Logger.hpp"

#include <csignal>

using namespace NetWork;

static void OnMessage(ConnectionPtr conn) {
    Buffer* buf = conn->ReadBuffer();
    if (buf->ReadAbleSize() > 0) {
        // echo: 读多少回多少
        std::string str = buf->ReadAsStringAndPop(buf->ReadAbleSize());
        conn->Send(str.data(), str.size());
    }
}

static void OnConnection(ConnectionPtr conn) {
    if (conn->isConnected()) {
        LOG(INFO) << "client connected: " << conn->PeerAddr().StrAddr();
    } else {
        LOG(INFO) << "client disconnected: " << conn->PeerAddr().StrAddr();
    }
}

int main() {
    const int port = 8080;

    NetWork::EventLoop loop;
    TcpServer server(&loop, port, "EchoServer");

    server.setExtraThread(4);
    server.setMessageCallback(OnMessage);
    server.setConnectionCallback(OnConnection);

    // SIGINT/SIGTERM 优雅退出 (signal handler 在 loop 线程运行, 调用 stop() 退出 loop)
    loop.signalHandler().Register(SIGINT, [&server]() {
        LOG(INFO) << "Received SIGINT, stopping...";
        server.stop();
    });
    loop.signalHandler().Register(SIGTERM, [&server]() {
        LOG(INFO) << "Received SIGTERM, stopping...";
        server.stop();
    });

    LOG(INFO) << "echo_server listening on :" << port;
    server.start();     // 准备 (创建从线程池 + 启动 acceptor), 不阻塞
    loop.loop();        // 阻塞: 主线程跑主 EventLoop
    LOG(INFO) << "server exited";

    return 0;
}
