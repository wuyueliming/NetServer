#include "../../src/server/base/Socket.hpp"
#include "../../src/server/base/InetAddr.hpp"
#include "../../src/server/base/LOGGER/log.h"
#include "../../src/server/base/Epoller.hpp"
#include"../../src/server/Channel.h"
#include"../../src/server/Acceptor.h"
#include"../../src/server/TcpServer.h"
#include"../../src/server/Reactor.h"
#include <iostream>
#include <cstring>
#include <string>

int main(){
    const int port = 8080;

    Aether::TcpServer server(port);
    server.start();

    return 0;
}
