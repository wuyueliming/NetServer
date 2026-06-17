#pragma once

#include <string>
#include <vector>
#include <regex>
#include <functional>
#include "Util.hpp"
#include "httpContext.hpp"
#include "httpResponse.hpp"
#include <Aether/TcpServer.hpp>
#include <Aether/Connection.hpp>

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
        Aether::TcpServer _server;
        HttpCodec _codec;                  // HTTP 编解码器
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
        void WriteResponse(const Aether::ConnectionPtr &conn, const HttpRequest &req, HttpResponse &rsp) {
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
        //消息回调：从 FrameDecoder 获取完整帧，用 HttpCodec 解析
        void OnMessage(const Aether::ConnectionPtr &conn) {
            while (conn->HasMessage()) {
                std::string frame = conn->Recv();

                // 空帧表示解析错误
                if (frame.empty()) {
                    HttpRequest req;
                    HttpResponse rsp(400);
                    ErrorHandler(req, &rsp);
                    WriteResponse(conn, req, rsp);
                    conn->Shutdown();
                    return;
                }

                // 用 HttpCodec 解码帧
                HttpRequest req = HttpCodec::Decode(frame);
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
            }
        }
    public:
        HttpServer(int port, int timeout = DEFAULT_TIMEOUT):_server(port) {
            _server.EnableInactiveRelease(timeout);
            _server.SetFrameDecoderFactory([]() -> std::unique_ptr<Aether::FrameDecoder> {
                return std::make_unique<HttpFrameDecoder>();
            });
            _server.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1));
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
            _server.SetThreadCount(count);
        }
        void Listen() {
            _server.start();
        }
};
