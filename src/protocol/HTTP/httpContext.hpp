#pragma once

#include <string>
#include <vector>
#include <regex>
#include <queue>
#include "Util.hpp"
#include "httpRequest.hpp"
#include "../../server/base/BufferReader.hpp"
#include "../../server/ProtocolContext.hpp"

typedef enum {
    RECV_HTTP_ERROR,
    RECV_HTTP_LINE,
    RECV_HTTP_HEAD,
    RECV_HTTP_BODY,
    RECV_HTTP_OVER
}HttpRecvStatus;

static constexpr int kMaxLine = 8192;

class HttpContext : public Aether::ProtocolContext {
public:
    HttpContext():_resp_status(200), _recv_status(RECV_HTTP_LINE) {}

    void Parse(Aether::BufferReader &reader) override {
        while (reader.ReadAbleSize() > 0 && _resp_status < 400) {
            switch(_recv_status) {
                case RECV_HTTP_LINE:
                    if (!RecvHttpLine(reader)) break;
                    [[fallthrough]];
                case RECV_HTTP_HEAD:
                    if (!RecvHttpHead(reader)) break;
                    [[fallthrough]];
                case RECV_HTTP_BODY:
                    RecvHttpBody(reader);
                    break;
                default: break;
            }
            if (_recv_status == RECV_HTTP_OVER) {
                _requests.push(std::move(_request));
                Reset();
            } else {
                break;
            }
        }
        if (_resp_status >= 400) {
            // 只丢弃当前请求可能占用的数据，而非全部缓冲区
            // 由于无法精确判断当前请求边界，保守丢弃全部以避免死循环
            // TODO: 更精确的错误恢复策略
            reader.MoveReadOffset(reader.ReadAbleSize());
        }
    }

    bool HasMessage() const override { return !_requests.empty(); }

    std::any PopMessage() override {
        HttpRequest req = std::move(_requests.front());
        _requests.pop();
        return req;
    }

    int ErrorStatus() const override { return _resp_status; }

    std::any GetPartialMessage() const override { return _request; }

    void Reset() override {
        _resp_status = 200;
        _recv_status = RECV_HTTP_LINE;
        _request.ReSet();
    }

private:
    bool ParseHttpLine(const std::string &line) {
        std::smatch matches;
        static const std::regex e("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?", std::regex::icase);
        bool ret = std::regex_match(line, matches, e);
        if (ret == false) {
            _recv_status = RECV_HTTP_ERROR;
            _resp_status = 400;
            return false;
        }
        _request._method = matches[1];
        std::transform(_request._method.begin(), _request._method.end(), _request._method.begin(), ::toupper);
        _request._path = Util::UrlDecode(matches[2], false);
        _request._version = matches[4];
        std::vector<std::string> query_string_arry;
        std::string query_string = matches[3];
        Util::Split(query_string, "&", &query_string_arry);
        for (auto &str : query_string_arry) {
            size_t pos = str.find("=");
            std::string key, val;
            if (pos == std::string::npos) {
                // 无值参数如 ?flag 也是合法的
                key = Util::UrlDecode(str, true);
                val.clear();
            } else {
                key = Util::UrlDecode(str.substr(0, pos), true);
                val = Util::UrlDecode(str.substr(pos + 1), true);
            }
            _request.SetParam(key, val);
        }
        return true;
    }

    bool RecvHttpLine(Aether::BufferReader &reader) {
        if (_recv_status != RECV_HTTP_LINE) return false;
        std::string line = reader.GetLineAndPop();
        if (line.size() == 0) {
            if (reader.ReadAbleSize() > kMaxLine) {
                _recv_status = RECV_HTTP_ERROR;
                _resp_status = 414;
                return false;
            }
            // 缓冲区有数据但返回空行 → 空请求行，协议错误
            if (reader.ReadAbleSize() > 0) {
                _recv_status = RECV_HTTP_ERROR;
                _resp_status = 400;
                return false;
            }
            return true; // 等待更多数据
        }
        if (line.size() > kMaxLine) {
            _recv_status = RECV_HTTP_ERROR;
            _resp_status = 414;
            return false;
        }
        bool ret = ParseHttpLine(line);
        if (ret == false) {
            return false;
        }
        _recv_status = RECV_HTTP_HEAD;
        return true;
    }

    bool RecvHttpHead(Aether::BufferReader &reader) {
        if (_recv_status != RECV_HTTP_HEAD) return false;
        while(1){
            std::string line = reader.GetLineAndPop();
            if (line.size() == 0) {
                // GetLineAndPop 已剥离换行符，空行表示头部结束的空行分隔符
                // 或者缓冲区暂无数据
                if (reader.ReadAbleSize() > kMaxLine) {
                    _recv_status = RECV_HTTP_ERROR;
                    _resp_status = 414;
                    return false;
                }
                // 缓冲区有数据但返回空行 → 空行分隔符已被消费，头部结束
                if (reader.ReadAbleSize() > 0 || !_request._headers.empty()) {
                    _recv_status = RECV_HTTP_BODY;
                    return true;
                }
                return true; // 等待更多数据
            }
            if (line.size() > kMaxLine) {
                _recv_status = RECV_HTTP_ERROR;
                _resp_status = 414;
                return false;
            }
            // 空行分隔符（\r\n 或 \n），头部解析结束
            if (line == "\r\n" || line == "\n") {
                _recv_status = RECV_HTTP_BODY;
                return true;
            }
            bool ret = ParseHttpHead(line);
            if (ret == false) {
                return false;
            }
        }
        _recv_status = RECV_HTTP_BODY;
        return true;
    }

    bool ParseHttpHead(const std::string &line) {
        std::string head = line;
        if (head.back() == '\n') head.pop_back();
        if (head.back() == '\r') head.pop_back();
        size_t pos = head.find(':');
        if (pos == std::string::npos) {
            _recv_status = RECV_HTTP_ERROR;
            _resp_status = 400;
            return false;
        }
        std::string key = head.substr(0, pos);
        // 去除值部分的前导空白
        std::string val = head.substr(pos + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        _request.SetHeader(key, val);
        return true;
    }

    bool RecvHttpBody(Aether::BufferReader &reader) {
        if (_recv_status != RECV_HTTP_BODY) return false;
        size_t content_length = _request.ContentLength();
        if (content_length == 0) {
            _recv_status = RECV_HTTP_OVER;
            return true;
        }
        size_t real_len = content_length - _request._body.size();
        if (reader.ReadAbleSize() >= real_len) {
            std::string body_data = reader.ReadAsStringAndPop(real_len);
            _request._body.append(body_data);
            _recv_status = RECV_HTTP_OVER;
            return true;
        }
        size_t readable = reader.ReadAbleSize();
        std::string body_data = reader.ReadAsStringAndPop(readable);
        _request._body.append(body_data);
        return true;
    }

private:
    int _resp_status;
    HttpRecvStatus _recv_status;
    HttpRequest _request;
    std::queue<HttpRequest> _requests;
};
