#include "../../src/server/base/TcpSocket.hpp"
#include "../../src/server/base/InetAddr.hpp"
#include "../../src/server/base/LOGGER/log.h"
#include "../../src/server/TcpServer.h"
#include "../../src/server/Connection.h"
#include "../../src/server/RawProtocolContext.hpp"
#include <iostream>
#include <cstring>
#include <string>

using namespace Aether;

TcpServer* g_server = nullptr;

void OnConnected(ConnectionPtr conn){
    conn->SetContext(std::make_shared<RawProtocolContext>());
    LOG(INFO) << "New connection from: " << conn->PeerAddr().StrAddr();
}

void OnMessage(ConnectionPtr conn){
    while (conn->HasMessage()) {
        std::any msg = conn->Recv();
        std::string str = std::any_cast<std::string>(std::move(msg));
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

    TcpServer server(port);
    g_server = &server;

    server.SetConnectedCallback(OnConnected);
    server.SetMessageCallback(OnMessage);
    server.SetClosedCallback(OnClosed);
    server.AddTimedTask(HeartbeatTask, 1);
    server.start();

    return 0;
}
