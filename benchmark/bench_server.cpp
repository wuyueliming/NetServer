#include "../src/protocol/HTTP/httpServer.hpp"
#include "../src/server/base/LOGGER/log.h"
#include <string>

using namespace Aether;

int main() {
    HttpServer server(8080, 60);
    server.SetThreadCount(4);
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

    LOG(INFO) << "Bench Server starting on port 8080 (4 IO threads)";
    server.Listen();

    return 0;
}
