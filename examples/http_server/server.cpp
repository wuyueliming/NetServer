// HTTP 服务器示例
// 使用 NetWork 网络库构建高性能 HTTP 服务器

#include <NetWork/NetWork.hpp>
#include "httpServer.hpp"

int main() {
    // 创建事件循环
    NetWork::EventLoop loop;

    // 创建 HTTP 服务器，监听 8080 端口
    HttpServer server(&loop, 8080);

    // 设置静态资源目录（可选）
    // server.SetBaseDir("./www");

    // 设置工作线程数（默认为 CPU 核数）
    server.SetExtraThread(4);

    // 注册路由处理器
    server.Get("/", [](const HttpRequest &req, HttpResponse *rsp) {
        rsp->SetContent("<html><body><h1>Welcome to NetWork HTTP Server</h1></body></html>");
    });
    
    server.Get("/api/hello", [](const HttpRequest &req, HttpResponse *rsp) {
        rsp->SetContent("{\"message\": \"Hello, World!\"}", "application/json");
    });
    
    server.Post("/api/data", [](const HttpRequest &req, HttpResponse *rsp) {
        // 处理 POST 请求
        rsp->SetContent("{\"status\": \"ok\"}", "application/json");
    });
    
    // 启动服务器
    LOG(INFO) << "HTTP Server starting on port 8080";
    server.Listen();  // Listen() 内部调用 start()，阻塞
    
    return 0;
}