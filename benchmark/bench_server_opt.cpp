#include "../examples/http_server/httpServer.hpp"

// 使用 Log_bench.hpp 的 NullLogStrategy 替代核心 LOGGER（benchmark 模式消除日志开销）
#undef LOG
#include "Log_bench.hpp"

#include <string>
#include <cstdlib>

using namespace NetWork;

int main() {
    // 性能测试版本：禁用日志以减少开销
    NetServer::LogModule::Enable_Null_Log_Strategy();

    // 通过环境变量配置（默认值: port=8080, threads=4, timeout=60）
    auto get_env = [](const char *name, int default_val) {
        const char *val = std::getenv(name);
        return val ? std::stoi(val) : default_val;
    };

    int port = get_env("BENCH_PORT", 8080);
    int threads = get_env("BENCH_THREADS", 4);
    int timeout = get_env("BENCH_TIMEOUT", 60);

    NetWork::EventLoop loop;
    HttpServer server(&loop, port, timeout);
    server.SetThreadCount(threads);
    server.SetBaseDir("./wwwroot");

    server.Get("/hello", [](const HttpRequest &req, HttpResponse *rsp) {
        rsp->SetContent("Hello World!", "text/plain");
    });

    server.Get("/ping", [](const HttpRequest &req, HttpResponse *rsp) {
        rsp->SetContent("pong", "text/plain");
    });

    server.Get("/data", [](const HttpRequest &req, HttpResponse *rsp) {
        static const std::string body(1024, 'X');
        rsp->SetContent(body, "text/plain");
    });

    server.Listen();  // Listen() 内部调用 start()，仅开始监听
    loop.loop();      // 用户自行调用，阻塞在此

    return 0;
}
