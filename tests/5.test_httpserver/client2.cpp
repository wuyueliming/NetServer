/*超时连接测试1：创建一个客户端，给服务器发送一次数据后，不动了，查看服务器是否会正常的超时关闭连接*/

#include <iostream>
#include <string>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "SOCKET ERROR" << std::endl;
        return -1;
    }

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret != 0) {
        std::cerr << "CONNECT ERROR" << std::endl;
        close(sock);
        return -1;
    }

    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
    while(1) {
        if (send(sock, req.c_str(), req.size(), 0) == -1) {
            std::cerr << "SEND ERROR" << std::endl;
            break;
        }
        char buf[1024] = {0};
        ssize_t n = recv(sock, buf, 1023, 0);
        if (n <= 0) {
            std::cout << "[INFO] Server closed connection (expected timeout)" << std::endl;
            break;
        }
        std::cout << "[RECV] " << buf << std::endl;
        std::cout << "[INFO] sleeping 65 seconds, waiting for server timeout..." << std::endl;
        sleep(65);
    }
    close(sock);
    return 0;
}
