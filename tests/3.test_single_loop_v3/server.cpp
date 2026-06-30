#include "../../src/common/TcpSocket.hpp"
#include "../../src/common/InetAddr.hpp"
#include "../../src/common/Logger.hpp"
#include "../../src/server/TcpServer.h"
#include "../../src/common/Connection.h"
#include <iostream>
#include <cstring>
#include <string>

using namespace NetWork;

TcpServer* g_server = nullptr;

void OnConnected(ConnectionPtr conn){
    LOG(INFO) << "New connection from: " << conn->PeerAddr().StrAddr();
}

void OnMessage(ConnectionPtr conn){
    Buffer* buf = conn->ReadBuffer();
    if (buf->ReadAbleSize() > 0) {
        std::string str = buf->ReadAsStringAndPop(buf->ReadAbleSize());
        LOG(INFO) << "recv: " << str;
        conn->Send(str.c_str(), str.size());
    }
}

void OnClosed(ConnectionPtr conn){
    LOG(INFO) << "Connection closed: " << conn->PeerAddr().StrAddr();
}

void HeartbeatTask(){
    static int count = 0;
    LOG(INFO) << "Server is alive, heartbeat: " << ++count;
    if(g_server){
        g_server->AddTimedTask(HeartbeatTask, 1);
    }
}

int main(){
    const int port = 8080;

    NetWork::EventLoop loop;
    TcpServer server(&loop, port);
    g_server = &server;

    server.setConnectionCallback(OnConnected);
    server.setMessageCallback(OnMessage);
    server.setCloseCallback(OnClosed);
    server.AddTimedTask(HeartbeatTask, 1);
    server.start();  // 仅开始监听，不创建线程
    loop.loop();     // 用户自行调用，阻塞在此

    return 0;
}