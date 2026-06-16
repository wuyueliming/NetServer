#include "../../src/common/base/TcpSocket.hpp"
#include "../../src/common/base/InetAddr.hpp"
#include "../../src/common/base/Logger.hpp"
#include "../../src/server/TcpServer.h"
#include "../../src/common/Connection.h"
#include "../../src/common/FrameDecoder.hpp"
#include <iostream>
#include <cstring>
#include <string>

using namespace Aether;

TcpServer* g_server = nullptr;

void OnConnected(ConnectionPtr conn){
    LOG(INFO) << "New connection from: " << conn->PeerAddr().StrAddr();
}

void OnMessage(ConnectionPtr conn){
    while (conn->HasMessage()) {
        std::string str = conn->Recv();
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

    server.SetFrameDecoderFactory([]() -> std::unique_ptr<Aether::FrameDecoder> {
        return std::make_unique<Aether::NoopFrameDecoder>();
    });
    server.SetConnectedCallback(OnConnected);
    server.SetMessageCallback(OnMessage);
    server.SetClosedCallback(OnClosed);
    server.AddTimedTask(HeartbeatTask, 1);
    server.start();

    return 0;
}
