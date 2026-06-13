#pragma once

#include <string>
#include <unordered_map>
#include <regex>

class HttpRequest {
    public:
        std::string _method;      //请求方法
        std::string _path;        //资源路径
        std::string _version;     //协议版本
        std::string _body;        //请求正文
        std::smatch _matches;     //资源路径的正则提取数据
        std::unordered_map<std::string, std::string> _headers;  //头部字段
        std::unordered_map<std::string, std::string> _params;   //查询字符串
    public:
        HttpRequest():_version("HTTP/1.1") {}
        void ReSet() {
            _method.clear();
            _path.clear();
            _version = "HTTP/1.1";
            _body.clear();
            std::smatch match;
            _matches.swap(match);
            _headers.clear();
            _params.clear();
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
        //插入查询字符串
        void SetParam(const std::string &key, const std::string &val) {
            _params.insert(std::make_pair(key, val));
        }
        //判断是否有某个指定的查询字符串
        bool HasParam(const std::string &key) const {
            auto it = _params.find(key);
            if (it == _params.end()) {
                return false;
            }
            return true;
        }
        //获取指定的查询字符串
        std::string GetParam(const std::string &key) const {
            auto it = _params.find(key);
            if (it == _params.end()) {
                return "";
            }
            return it->second;
        }
        //获取正文长度
        size_t ContentLength() const {
            // Content-Length: 1234\r\n
            bool ret = HasHeader("Content-Length");
            if (ret == false) {
                return 0;
            }
            std::string clen = GetHeader("Content-Length");
            return std::stol(clen);
        }
        //判断是否是短链接
        bool Close() const {
            // 没有Connection字段，或者有Connection但是值是close，则都是短链接，否则就是长连接
            if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive") {
                return false;
            }
            return true;
        }
};
