#pragma once

#include <string>
#include <unordered_map>

class HttpResponse {
    public:
        int _statu;
        bool _redirect_flag;
        std::string _body;
        std::string _redirect_url;
        std::unordered_map<std::string, std::string> _headers;
    public:
        HttpResponse():_statu(200), _redirect_flag(false) {}
        HttpResponse(int statu):_statu(statu), _redirect_flag(false) {} 
        void ReSet() {
            _statu = 200;
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
        bool HasHeader(const std::string &key) {
            auto it = _headers.find(key);
            if (it == _headers.end()) {
                return false;
            }
            return true;
        }
        //获取指定头部字段的值
        std::string GetHeader(const std::string &key) {
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
        void SetRedirect(const std::string &url, int statu = 302) {
            _statu = statu;
            _redirect_flag = true;
            _redirect_url = url;
        }
        //判断是否是短链接
        bool Close() {
            // 没有Connection字段，或者有Connection但是值是close，则都是短链接，否则就是长连接
            if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive") {
                return false;
            }
            return true;
        }
};
