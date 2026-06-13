#include "../../src/protocol/HTTP/httpServer.hpp"
#include "../../src/server/base/LOGGER/log.h"
#include <string>

using namespace Aether;

void Hello(const HttpRequest &req, HttpResponse *rsp) {
    rsp->SetContent("Hello World!", "text/plain");
}

void Numbers(const HttpRequest &req, HttpResponse *rsp) {
    std::string body;
    for(int i = 0; i < 10; i++) {
        body += std::to_string(i) + "\n";
    }
    rsp->SetContent(body, "text/plain");
}

void Echo(const HttpRequest &req, HttpResponse *rsp) {
    if (req.HasParam("msg") == false) {
        rsp->_statu = 400;
        return;
    }
    const std::string &msg = req.GetParam("msg");
    rsp->SetContent(msg, "text/plain");
}

int main(){
    HttpServer server(8080);
    server.SetBaseDir("./wwwroot");
    // server.SetThreadCount(3);

    server.Get("/hello", Hello);
    server.Get("/numbers", Numbers);
    server.Get("/echo", Echo);

    LOG(INFO) << "HTTP Server started on port 8080";
    server.Listen();

    return 0;
}
