#pragma once

#include <string>
#include <unordered_map>

class HttpResponse {
    public:
        int _status;
        bool _redirect_flag;
        std::string _body;
        std::string _redirect_url;
        std::unordered_map<std::string, std::string> _headers;
    public:
        HttpResponse():_status(200), _redirect_flag(false) {}
        HttpResponse(int status):_status(status), _redirect_flag(false) {} 
        void ReSet() {
            _status = 200;
            _redirect_flag = false;
            _body.clear();
            _redirect_url.clear();
            _headers.clear();
        }
        //插入头部字段
        void SetHeader(const std::string &key, const std::string &val) {
            _headers.insert(std::make_pair(key, val));
        }
        //判断是否存在指定头部字段
        bool HasHeader(const std::string &key) const {
            auto it = _headers.find(key);
            if (it == _headers.end()) {
                return false;
            }
            return true;
        }
        //获取指定头部字段的值
        std::string GetHeader(const std::string &key) const {
            auto it = _headers.find(key);
            if (it == _headers.end()) {
                return "";
            }
            return it->second;
        }
        void SetContent(const std::string &body,  const std::string &type = "text/html") {
            _body = body;
            SetHeader("Content-Type", type);
        }
        void SetRedirect(const std::string &url, int status = 302) {
            _status = status;
            _redirect_flag = true;
            _redirect_url = url;
        }
        //判断是否是短链接
        bool Close() const {
            // HTTP/1.1 默认是长连接，只有显式指定 Connection: close 时才关闭
            if (HasHeader("Connection") == true && GetHeader("Connection") == "close") {
                return true;
            }
            return false;
        }
};
