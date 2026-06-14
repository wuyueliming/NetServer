#include "../../src/server/base/TcpSocket.hpp"
#include "../../src/server/base/InetAddr.hpp"
#include "../../src/server/base/LOGGER/log.h"
#include "../../src/server/TcpServer.h"
#include "../../src/server/Connection.h"
#include "../../src/protocol/RawProtocolContext.hpp"
#include <iostream>
#include <cstring>
#include <string>

using namespace Aether;

void OnConnected(ConnectionPtr conn){
    conn->SetContext(std::make_shared<RawProtocolContext>());
}

void OnMessage(ConnectionPtr conn){
    while (conn->HasMessage()) {
        std::any msg = conn->Recv();
        std::string str = std::any_cast<std::string>(std::move(msg));
        LOG(INFO) << "recv: " << str;
        conn->Send(str.c_str(), str.size());
    }
}

int main(){
    const int port = 8080;

    TcpServer server(port);
    server.SetConnectedCallback(OnConnected);
    server.SetMessageCallback(OnMessage);
    server.start();

    return 0;
}
