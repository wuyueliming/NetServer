#pragma once

#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<string>



namespace NetWork{

    class InetAddr{
        using string = std::string;
    public:
        ///默认构造函数
        InetAddr() : _addr{} {
        }
        //网络序列ip和port的构造函数
        InetAddr(struct sockaddr_in addr){
            _addr = addr;
        }
        ///主机序列ip和port的构造函数
        InetAddr(int port, string ip="0.0.0.0"){
            _addr.sin_family = AF_INET;
            _addr.sin_port = htons(port);
            // 使用 inet_pton 替代 inet_addr（inet_addr 无法区分错误与广播地址）
            if (inet_pton(AF_INET, ip.c_str(), &_addr.sin_addr) <= 0) {
                // 转换失败，默认设置为 0.0.0.0
                _addr.sin_addr.s_addr = INADDR_ANY;
            }
        }
        ~InetAddr(){
        }
        sockaddr_in* GetAddr(){
            return &_addr;
        }
        const sockaddr_in* GetAddr() const {
            return &_addr;
        }
        string StrAddr() const {
            char buf[32];
            string addr = inet_ntop(AF_INET,&_addr.sin_addr,buf,32);
            addr += ":" + std::to_string(ntohs(_addr.sin_port));
            return addr;
        }

        int HostPort() const {
            return ntohs(_addr.sin_port);
        }
        int NetPort() const {
            return _addr.sin_port;
        }
        uint32_t HostIp() const {
            return ntohl(_addr.sin_addr.s_addr);
        }
        uint32_t NetIp() const {
            return _addr.sin_addr.s_addr;
        }

        bool operator==(const InetAddr& other) const {
            return _addr.sin_port == other._addr.sin_port &&
                   _addr.sin_addr.s_addr == other._addr.sin_addr.s_addr;
        }

    private:
        sockaddr_in _addr;
    };



}

