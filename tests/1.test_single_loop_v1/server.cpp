#include "../../src/common/base/Socket.hpp"
#include "../../src/common/base/InetAddr.hpp"
#include "../../src/common/base/Logger.hpp"
#include "../../src/common/base/Epoller.hpp"
#include"../../src/common/Channel.h"
#include"../../src/server/Acceptor.h"
#include"../../src/server/TcpServer.h"
#include"../../src/common/Reactor.h"
#include <iostream>
#include <cstring>
#include <string>

int main(){
    const int port = 8080;

    Aether::TcpServer server(port);
    server.start();

    return 0;
}
