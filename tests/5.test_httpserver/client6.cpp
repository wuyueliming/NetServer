/*大文件传输测试，给服务器上传一个大文件，服务器将文件保存下来，观察处理结果*/
/*
    上传的文件，和服务器保存的文件一致
*/
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>
#include <sys/stat.h>

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

    //读取测试文件
    std::string body;
    std::ifstream ifs("./hello.txt", std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "OPEN hello.txt FAILED! Create a test file first." << std::endl;
        close(sock);
        return -1;
    }
    ifs.seekg(0, ifs.end);
    body.resize(ifs.tellg());
    ifs.seekg(0, ifs.beg);
    ifs.read(&body[0], body.size());
    ifs.close();

    std::string req = "PUT /1234.txt HTTP/1.1\r\nConnection: keep-alive\r\n";
    req += "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    if (send(sock, req.c_str(), req.size(), 0) == -1) {
        std::cerr << "SEND HEADER ERROR" << std::endl;
        close(sock);
        return -1;
    }
    if (send(sock, body.c_str(), body.size(), 0) == -1) {
        std::cerr << "SEND BODY ERROR" << std::endl;
        close(sock);
        return -1;
    }
    char buf[1024] = {0};
    ssize_t n = recv(sock, buf, 1023, 0);
    if (n > 0) {
        std::cout << "[RECV] " << buf << std::endl;
    }
    sleep(3);
    close(sock);
    return 0;
}
