// multi_client.cpp - 多连TcpClient测试
//
// 验证多host连接 + GetConnection round-robin + GetConnection(addr)定向
// 验证setExtraThread多IO线程
//
// 流程:
//   1. 启动3个echo server (8091/8092/8093), 各跑独立线程
//   2. 创建多连TcpClient, setExtraThread(2), connect到3个host
//   3. 验证3连接建立, getAllConnections().size()==3
//   4. 验证GetConnection() round-robin命中>=2个不同连接
//   5. 验证GetConnection(addr)精确定位
//   6. 发送12条消息 (3定向 + 9 round-robin), 验证收到12条echo

#include "../../src/common/EventLoop.h"
#include "../../src/server/TcpServer.h"
#include "../../src/client/TcpClient.h"
#include "../../src/common/Connection.h"
#include "../../src/common/InetAddr.hpp"
#include "../../src/common/Logger.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

using namespace NetWork;

//全局计数
static std::atomic<int> g_conn_up{0};
static std::atomic<int> g_echo_count{0};
static std::mutex g_mtx;
static std::condition_variable g_cv;

//Echo server回调
static void OnServerMessage(ConnectionPtr conn) {
    Buffer* buf = conn->ReadBuffer();
    if (buf->ReadAbleSize() > 0) {
        std::string str = buf->ReadAsStringAndPop(buf->ReadAbleSize());
        conn->Send(str.data(), str.size());
    }
}

//Server上下文
struct ServerCtx {
    int port;
    NetWork::EventLoop* loop{nullptr};
    std::atomic<bool> ready{false};
};

static void RunServer(ServerCtx* ctx) {
    NetWork::EventLoop loop;
    TcpServer server(&loop, ctx->port, "EchoSrv-" + std::to_string(ctx->port));
    server.setExtraThread(0);
    server.setMessageCallback(OnServerMessage);
    server.start();
    ctx->loop = &loop;
    ctx->ready.store(true);
    loop.loop();
}

//Client回调
static void OnClientMessage(ConnectionPtr conn) {
    Buffer* buf = conn->ReadBuffer();
    if (buf->ReadAbleSize() > 0) {
        buf->ReadAsStringAndPop(buf->ReadAbleSize());
        g_echo_count.fetch_add(1);
    }
}

static void OnClientConnection(ConnectionPtr conn) {
    if (conn->isConnected()) {
        g_conn_up.fetch_add(1);
        g_cv.notify_one();
    } else {
        g_conn_up.fetch_sub(1);
    }
}

int main() {
    std::cout << "=== Multi-Client Test ===" << std::endl;

    //1. 启动3个echo server
    const std::vector<int> ports = {8091, 8092, 8093};
    std::vector<ServerCtx> servers(ports.size());
    std::vector<std::thread> server_threads;

    for (size_t i = 0; i < ports.size(); i++) {
        servers[i].port = ports[i];
        server_threads.emplace_back(RunServer, &servers[i]);
    }
    for (auto& s : servers) {
        for (int k = 0; k < 30 && !s.ready.load(); k++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    std::cout << "[+] 3 echo servers started on 8091/8092/8093" << std::endl;

    //2. 创建多连TcpClient
    NetWork::EventLoop loop;
    std::vector<InetAddr> addrs;
    for (int p : ports) addrs.emplace_back(p, "127.0.0.1");

    TcpClient client(&loop, addrs, "MultiClient");
    client.setExtraThread(2);
    client.setMessageCallback(OnClientMessage);
    client.setConnectionCallback(OnClientConnection);

    //3. 启动主loop + connect
    std::thread loop_thread([&loop]() { loop.loop(); });
    client.connect();

    //等待3连接建立
    {
        std::unique_lock<std::mutex> lk(g_mtx);
        g_cv.wait_for(lk, std::chrono::seconds(3), []() {
            return g_conn_up.load() >= 3;
        });
    }
    int conns = g_conn_up.load();
    std::cout << "[+] connections established: " << conns << " / 3" << std::endl;

    //4. 验证getAllConnections()
    auto all_conns = client.getAllConnections();
    std::cout << "[+] getAllConnections().size() = " << all_conns.size() << std::endl;
    for (size_t i = 0; i < all_conns.size(); i++) {
        std::cout << "    conn[" << i << "] id=" << all_conns[i]->ID()
                  << " peer=" << all_conns[i]->PeerAddr().StrAddr()
                  << " connected=" << (all_conns[i]->isConnected() ? "yes" : "no") << std::endl;
    }

    //5. 验证GetConnection() round-robin
    //每个host有独立nextConnId, ID可能相同, 用peer地址区分
    std::set<std::string> picked_peers;
    for (int i = 0; i < 6; i++) {
        auto c = client.GetConnection();
        if (c) {
            picked_peers.insert(c->PeerAddr().StrAddr());
            std::cout << "    pick[" << i << "] id=" << c->ID()
                      << " peer=" << c->PeerAddr().StrAddr() << std::endl;
        } else {
            std::cout << "    pick[" << i << "] null" << std::endl;
        }
    }
    std::cout << "[+] GetConnection() 6 calls -> distinct peers: " << picked_peers.size() << std::endl;

    //6. 验证GetConnection(addr)定位
    auto conn8092 = client.GetConnection(InetAddr(8092, "127.0.0.1"));
    bool addr_ok = (conn8092 && conn8092->isConnected());
    std::cout << "[+] GetConnection(8092) = " << (addr_ok ? "ok" : "null/disconnected") << std::endl;

    //7. 发送消息并验证echo
    int echo_before = g_echo_count.load();

    //3条定向消息
    for (int p : ports) {
        auto c = client.GetConnection(InetAddr(p, "127.0.0.1"));
        if (c && c->isConnected()) {
            c->Send("addr", 4);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    //9条round-robin消息
    for (int i = 0; i < 9; i++) {
        auto c = client.GetConnection();
        if (c && c->isConnected()) {
            c->Send("pick", 4);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }

    //等待echo到达
    int expected = echo_before + 12;
    for (int i = 0; i < 50; i++) {
        if (g_echo_count.load() >= expected) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    int total_echo = g_echo_count.load() - echo_before;
    std::cout << "[+] echoes received: " << total_echo << " / 12" << std::endl;

    //8. 结果汇总
    bool ok = (conns >= 3)
              && (all_conns.size() == 3)
              && (picked_peers.size() >= 2)
              && addr_ok
              && (total_echo == 12);

    std::cout << "\n=== Result ===" << std::endl;
    std::cout << "  3 connections established:    " << (conns >= 3 ? "PASS" : "FAIL") << std::endl;
    std::cout << "  getAllConnections().size() == 3: " << (all_conns.size() == 3 ? "PASS" : "FAIL") << std::endl;
    std::cout << "  GetConnection round-robin:   " << (picked_peers.size() >= 2 ? "PASS" : "FAIL") << std::endl;
    std::cout << "  GetConnection(addr) lookup:      " << (addr_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "  12 echoes received:           " << (total_echo == 12 ? "PASS" : "FAIL") << std::endl;
    std::cout << "\n=== " << (ok ? "Test Passed" : "Test FAILED") << " ===" << std::endl;

    //9. 清理
    client.stop();
    loop.Quit();
    loop_thread.join();

    for (auto& s : servers) {
        if (s.loop) s.loop->Quit();
    }
    for (auto& t : server_threads) {
        if (t.joinable()) t.join();
    }

    return ok ? 0 : 1;
}
