/*一次性给服务器发送多条数据，然后查看服务器的处理结果*/
/*每一条请求都应该得到正常处理*/

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

    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
    req += "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
    req += "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
    while(1) {
        assert(send(sock, req.c_str(), req.size(), 0) != -1);
        char buf[4096] = {0};
        assert(recv(sock, buf, 4095, 0) > 0);
        std::cout << "[RECV] " << buf << std::endl;
        sleep(3);
    }
    close(sock);
    return 0;
}
