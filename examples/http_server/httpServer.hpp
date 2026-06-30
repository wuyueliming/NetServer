#pragma once

#include <string>
#include <vector>
#include <regex>
#include <functional>
#include "Util.hpp"
#include "httpContext.hpp"
#include "httpResponse.hpp"
#include <NetWork/TcpServer.hpp>
#include <NetWork/Connection.hpp>

#define DEFAULT_TIMEOUT 60

class HttpServer {
    private:
        using Handler = std::function<void(const HttpRequest &, HttpResponse *)>;
        using Handlers = std::vector<std::pair<std::regex, Handler>>;
        Handlers _get_route;
        Handlers _post_route;
        Handlers _put_route;
        Handlers _delete_route;
        std::string _basedir; //静态资源根目录
        NetWork::TcpServer _server;
    private:
        void ErrorHandler(const HttpRequest &req, HttpResponse *rsp) {
            //1. 组织一个错误展示页面
            std::string body;
            body += "<html>";
            body += "<head>";
            body += "<meta http-equiv='Content-Type' content='text/html;charset=utf-8'>";
            body += "</head>";
            body += "<body>";
            body += "<h1>";
            body += std::to_string(rsp->_status);
            body += " ";
            body += Util::StatusDesc(rsp->_status);
            body += "</h1>";
            body += "</body>";
            body += "</html>";
            //2. 将页面数据，当作响应正文，放入rsp中
            rsp->SetContent(body, "text/html");
        }
        //将HttpResponse中的要素按照http协议格式进行组织，发送
        void WriteResponse(const NetWork::ConnectionPtr &conn, const HttpRequest &req, HttpResponse &rsp) {
            //1. 先完善头部字段
            if (req.Close() == true) {
                rsp.SetHeader("Connection", "close");
            }else {
                rsp.SetHeader("Connection", "keep-alive");
            }
            if (rsp._body.empty() == false && rsp.HasHeader("Content-Length") == false) {
                rsp.SetHeader("Content-Length", std::to_string(rsp._body.size()));
            }
            if (rsp._body.empty() == false && rsp.HasHeader("Content-Type") == false) {
                rsp.SetHeader("Content-Type", "application/octet-stream");
            }
            if (rsp._redirect_flag == true) {
                rsp.SetHeader("Location", rsp._redirect_url);
            }
            //2. 使用 HttpCodec 编码并发送
            std::string resp_frame = HttpCodec::Encode(rsp, req);
            conn->Send(resp_frame.c_str(), resp_frame.size());
        }
        bool IsFileHandler(const HttpRequest &req) {
            // 1. 必须设置了静态资源根目录
            if (_basedir.empty()) {
                return false;
            }
            // 2. 请求方法，必须是GET / HEAD请求方法
            if (req._method != "GET" && req._method != "HEAD") {
                return false;
            }
            // 3. 请求的资源路径必须是一个合法路径
            if (Util::ValidPath(req._path) == false) {
                return false;
            }
            // 4. 请求的资源必须存在,且是一个普通文件
            std::string req_path = _basedir + req._path;
            if (req._path.back() == '/')  {
                req_path += "index.html";
            }
            if (Util::IsRegular(req_path) == false) {
                return false;
            }
            return true;
        }
        //静态资源的请求处理
        void FileHandler(const HttpRequest &req, HttpResponse *rsp) {
            std::string req_path = _basedir + req._path;
            if (req._path.back() == '/')  {
                req_path += "index.html";
            }
            bool ret = Util::ReadFile(req_path, &rsp->_body);
            if (ret == false) {
                rsp->_status = 404;
                return;
            }
            std::string mime = Util::ExtMime(req_path);
            rsp->SetHeader("Content-Type", mime);
            return;
        }
        //功能性请求的分类处理
        void Dispatcher(HttpRequest &req, HttpResponse *rsp, Handlers &handlers) {
            for (auto &handler : handlers) {
                const std::regex &re = handler.first;
                const Handler &functor = handler.second;
                bool ret = std::regex_match(req._path, req._matches, re);
                if (ret == false) {
                    continue;
                }
                return functor(req, rsp);
            }
            rsp->_status = 404;
        }
        void Route(HttpRequest &req, HttpResponse *rsp) {
            if (IsFileHandler(req) == true) {
                return FileHandler(req, rsp);
            }
            if (req._method == "GET" || req._method == "HEAD") {
                return Dispatcher(req, rsp, _get_route);
            }else if (req._method == "POST") {
                return Dispatcher(req, rsp, _post_route);
            }else if (req._method == "PUT") {
                return Dispatcher(req, rsp, _put_route);
            }else if (req._method == "DELETE") {
                return Dispatcher(req, rsp, _delete_route);
            }
            rsp->_status = 405;
            return ;
        }
        //连接建立回调：设置 HttpContext 到连接的 context
        void OnConnected(const NetWork::ConnectionPtr &conn) {
            conn->setContext(HttpContext());
        }
        //消息回调：从 Buffer 解析 HTTP 请求
        void OnMessage(const NetWork::ConnectionPtr &conn) {
            auto* ctx = std::any_cast<HttpContext>(conn->getMutableContext());
            if (!ctx) {
                // context 未设置，发送错误响应
                HttpRequest empty_req;
                HttpResponse rsp(400);
                ErrorHandler(empty_req, &rsp);
                WriteResponse(conn, empty_req, rsp);
                conn->Shutdown();
                return;
            }
            NetWork::Buffer* buf = conn->ReadBuffer();
            while (buf->ReadAbleSize() > 0) {
                if (!ctx->ParseRequest(buf)) {
                    // 解析错误
                    HttpRequest empty_req;
                    HttpResponse rsp(ctx->GetErrorStatus());
                    ErrorHandler(empty_req, &rsp);
                    WriteResponse(conn, empty_req, rsp);
                    conn->Shutdown();
                    return;
                }
                if (ctx->GotAll()) {
                    HttpRequest& req = ctx->GetRequest();
                    HttpResponse rsp(200);
                    Route(req, &rsp);
                    if (rsp._status >= 400) {
                        ErrorHandler(req, &rsp);
                    }
                    WriteResponse(conn, req, rsp);
                    if (rsp.Close() == true) {
                        conn->Shutdown();
                        return;
                    }
                    ctx->Reset();  // 重置状态，处理下一个请求
                } else {
                    // 数据不够，等待下次 OnMessage
                    break;
                }
            }
        }
    public:
        HttpServer(NetWork::EventLoop* loop, int port, int timeout = DEFAULT_TIMEOUT):_server(loop, port) {
            _server.enableInactiveRelease(timeout);
            _server.setConnectionCallback(std::bind(&HttpServer::OnConnected, this, std::placeholders::_1));
            _server.setMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1));
        }
        void SetBaseDir(const std::string &path) {
            if (Util::IsDirectory(path) == false) {
                LOG(ERROR) << "SetBaseDir failed: " << path << " is not a valid directory";
                return;
            }
            _basedir = path;
        }
        /*设置/添加，请求（请求的正则表达）与处理函数的映射关系*/
        void Get(const std::string &pattern, const Handler &handler) {
            _get_route.push_back(std::make_pair(std::regex(pattern), handler));
        }
        void Post(const std::string &pattern, const Handler &handler) {
            _post_route.push_back(std::make_pair(std::regex(pattern), handler));
        }
        void Put(const std::string &pattern, const Handler &handler) {
            _put_route.push_back(std::make_pair(std::regex(pattern), handler));
        }
        void Delete(const std::string &pattern, const Handler &handler) {
            _delete_route.push_back(std::make_pair(std::regex(pattern), handler));
        }
        void SetThreadCount(int count) {
            _server.setThreadNum(count);
        }
        void Listen() {
            _server.start();
        }
};