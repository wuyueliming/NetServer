/* 业务处理超时，查看服务器的处理情况
    当服务器达到了一个性能瓶颈，在一次业务处理中花费了太长的时间（超过了服务器设置的非活跃超时时间）
     1. 在一次业务处理中耗费太长时间，导致其他的连接也被连累超时，其他的连接有可能会被拖累超时释放
     假设现在  12345描述符就绪了， 在处理1的时候花费了30s处理完，超时了，导致2345描述符因为长时间没有刷新活跃度
       1. 如果接下来的2345描述符都是通信连接描述符，如果都就绪了，则并不影响，因为接下来就会进行处理并刷新活跃度
       2. 如果接下来的2号描述符是定时器事件描述符，定时器触发超时，执行定时任务，就会将345描述符给释放掉
          这时候一旦345描述符对应的连接被释放，接下来在处理345事件的时候就会导致程序崩溃（内存访问错误）
          因此这时候，在本次事件处理中，并不能直接对连接进行释放，而应该将释放操作压入到任务池中，
          等到事件处理完了执行任务池中的任务的时候，再去释放
*/

#include <iostream>
#include <string>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>

int main()
{
    signal(SIGCHLD, SIG_IGN);
    for (int i = 0; i < 10; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "FORK ERROR" << std::endl;
            return -1;
        }else if (pid == 0) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                std::cerr << "SOCKET ERROR" << std::endl;
                exit(1);
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
                exit(1);
            }

            std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
            int request_count = 0;
            while(request_count < 100) {
                if (send(sock, req.c_str(), req.size(), 0) == -1) {
                    std::cerr << "[CHILD " << i << "] SEND ERROR" << std::endl;
                    break;
                }
                char buf[1024] = {0};
                ssize_t n = recv(sock, buf, 1023, 0);
                if (n <= 0) {
                    std::cout << "[CHILD " << i << "] Server closed connection" << std::endl;
                    break;
                }
                std::cout << "[CHILD " << i << "] RECV OK (" << ++request_count << ")" << std::endl;
            }
            close(sock);
            exit(0);
        }
    }
    while(1) sleep(1);
    
    return 0;
}
