/*给服务器发送一个数据，告诉服务器要发送1024字节的数据，但是实际发送的数据不足1024，查看服务器处理结果*/
/*
    1. 如果数据只发送一次，服务器将得不到完整请求，就不会进行业务处理，客户端也就得不到响应，最终超时关闭连接
    2. 连着给服务器发送了多次 小的请求，  服务器会将后边的请求当作前边请求的正文进行处理，而后便处理的时候有可能就会因为处理错误而关闭连接
*/

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
    assert(sock >= 0);

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    assert(ret == 0);

    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 100\r\n\r\nbitejiuyeke";
    while(1) {
        assert(send(sock, req.c_str(), req.size(), 0) != -1);
        assert(send(sock, req.c_str(), req.size(), 0) != -1);
        assert(send(sock, req.c_str(), req.size(), 0) != -1);
        char buf[1024] = {0};
        ssize_t n = recv(sock, buf, 1023, 0);
        if (n <= 0) {
            std::cout << "[INFO] Server closed connection (expected)" << std::endl;
            break;
        }
        std::cout << "[RECV] " << buf << std::endl;
        sleep(3);
    }
    close(sock);
    return 0;
}
