#include "../../src/common/TcpSocket.hpp"
#include "../../src/common/InetAddr.hpp"
#include "../../src/common/Logger.hpp"
#include "../../src/server/TcpServer.h"
#include "../../src/common/Connection.h"
#include <iostream>
#include <cstring>
#include <string>

using namespace NetWork;

void OnMessage(ConnectionPtr conn){
    Buffer* buf = conn->ReadBuffer();
    if (buf->ReadAbleSize() > 0) {
        std::string str = buf->ReadAsStringAndPop(buf->ReadAbleSize());
        LOG(INFO) << "recv: " << str;
        conn->Send(str.c_str(), str.size());
    }
}

int main(){
    const int port = 8080;

    NetWork::EventLoop loop;
    TcpServer server(&loop, port);
    server.setMessageCallback(OnMessage);
    server.start();  // 仅开始监听，不创建线程
    loop.loop();     // 用户自行调用，阻塞在此

    return 0;
}