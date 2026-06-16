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

void OnMessage(ConnectionPtr conn){
    while (conn->HasMessage()) {
        std::string str = conn->Recv();
        LOG(INFO) << "recv: " << str;
        conn->Send(str.c_str(), str.size());
    }
}

int main(){
    const int port = 8080;

    TcpServer server(port);
    server.SetFrameDecoderFactory([]() -> std::unique_ptr<Aether::FrameDecoder> {
        return std::make_unique<Aether::NoopFrameDecoder>();
    });
    server.SetMessageCallback(OnMessage);
    server.start();

    return 0;
}
