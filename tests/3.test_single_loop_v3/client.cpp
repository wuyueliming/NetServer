#include "../../src/server/base/TcpSocket.hpp"
#include "../../src/server/base/InetAddr.hpp"
#include "../../src/server/base/LOGGER/log.h"
#include <iostream>
#include <cstring>

using namespace Aether;

int main()
{
    TcpSocket sock;
    InetAddr server_addr(8080, "127.0.0.1");

    if (!sock.create_client(server_addr)) {
        LOG(ERROR) << "connect failed";
        return -1;
    }

    LOG(INFO) << "Connected to server";

    char recv_buf[1024];

    while (true) {
        std::cout << "Enter message (or 'quit'): ";
        std::string line;
        std::getline(std::cin, line);

        if (line == "quit") break;

        sock.send(line.c_str(), line.size());
        LOG(INFO) << "Sent: " << line;

        memset(recv_buf, 0, sizeof(recv_buf));
        ssize_t n = sock.recv(recv_buf, sizeof(recv_buf) - 1);
        if (n > 0) {
            recv_buf[n] = '\0';
            LOG(INFO) << "Echo: " << recv_buf;
        }
        else {
            LOG(ERROR) << "recv error or server closed";
            break;
        }
    }
    LOG(INFO) << "Client exiting";
    return 0;
}
