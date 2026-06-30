#include "../../src/common/TcpSocket.hpp"
#include "../../src/common/InetAddr.hpp"
#include "../../src/common/Logger.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>

using namespace NetWork;

std::atomic<int> g_echo_count{0};

void ClientWorker(int id, const std::string &ip, int port, int msg_count) {
    TcpSocket sock;
    InetAddr server_addr(port, ip);
    if (!sock.create_client(server_addr)) {
        LOG(ERROR) << "client " << id << " connect failed";
        return;
    }
    LOG(INFO) << "client " << id << " connected";

    char recv_buf[4096];
    for (int i = 0; i < msg_count; i++) {
        std::string msg = "client" + std::to_string(id) + "_msg" + std::to_string(i);
        sock.send(msg.c_str(), msg.size());

        memset(recv_buf, 0, sizeof(recv_buf));
        ssize_t n = sock.recv(recv_buf, sizeof(recv_buf) - 1);
        if (n > 0) {
            recv_buf[n] = '\0';
            g_echo_count++;
        } else {
            LOG(ERROR) << "client " << id << " recv error";
            break;
        }
    }
    sock.Close();
    LOG(INFO) << "client " << id << " done";
}

int main() {
    const int port = 8080;
    const std::string ip = "127.0.0.1";
    int client_count = 5;
    int msg_per_client = 10;

    std::cout << "=== Echo Client Test ===" << std::endl;
    std::cout << "Clients: " << client_count << std::endl;
    std::cout << "Messages per client: " << msg_per_client << std::endl;

    std::vector<std::thread> threads;
    for (int i = 0; i < client_count; i++) {
        threads.emplace_back(ClientWorker, i, ip, port, msg_per_client);
    }

    for (auto &t : threads) {
        t.join();
    }

    std::cout << "=== Test Complete ===" << std::endl;
    std::cout << "Expected echoes: " << client_count * msg_per_client << std::endl;
    std::cout << "Actual echoes: " << g_echo_count.load() << std::endl;

    return 0;
}
