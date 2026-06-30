#pragma once

#include <string>
#include <vector>
#include <regex>
#include "Util.hpp"
#include "httpRequest.hpp"
#include "httpResponse.hpp"
#include <NetWork/Buffer.hpp>

// HttpContext：类似 muduo 的设计，直接从 Buffer 解析 HTTP 请求
// 应用层在 OnMessage 回调中调用 ParseRequest 处理粘包
class HttpContext {
public:
    HttpContext() : _resp_status(200), _recv_status(RECV_HTTP_LINE) {}

    // 从 Buffer 解析 HTTP 请求
    // 返回 true 表示解析成功（可能还没完成）
    // 返回 false 表示解析错误
    bool ParseRequest(NetWork::Buffer* buf) {
        while (buf->ReadAbleSize() > 0 && _resp_status < 400) {
            switch(_recv_status) {
                case RECV_HTTP_LINE:
                    if (!RecvHttpLine(buf)) break;
                    [[fallthrough]];
                case RECV_HTTP_HEAD:
                    if (!RecvHttpHead(buf)) break;
                    [[fallthrough]];
                case RECV_HTTP_BODY:
                    RecvHttpBody(buf);
                    break;
                default: break;
            }
            if (_recv_status == RECV_HTTP_OVER) {
                return true;
            } else {
                break;
            }
        }
        if (_resp_status >= 400) {
            buf->MoveReadOffset(buf->ReadAbleSize());
            return false;
        }
        return true;
    }

    // 是否解析完成
    bool GotAll() const { return _recv_status == RECV_HTTP_OVER; }

    // 获取解析结果
    HttpRequest& GetRequest() { return _request; }

    // 获取错误状态码
    int GetErrorStatus() const { return _resp_status; }

    // 重置状态（用于处理下一个请求）
    void Reset() {
        _resp_status = 200;
        _recv_status = RECV_HTTP_LINE;
        _request.ReSet();
    }

private:
    typedef enum {
        RECV_HTTP_ERROR,
        RECV_HTTP_LINE,
        RECV_HTTP_HEAD,
        RECV_HTTP_BODY,
        RECV_HTTP_OVER
    } HttpRecvStatus;

    static constexpr int kMaxLine = 8192;

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

    bool RecvHttpLine(NetWork::Buffer* buf) {
        if (_recv_status != RECV_HTTP_LINE) return false;
        std::string line = buf->GetLineAndPop();
        if (line.size() == 0) {
            if (buf->ReadAbleSize() > kMaxLine) {
                _recv_status = RECV_HTTP_ERROR;
                _resp_status = 414;
                return false;
            }
            if (buf->ReadAbleSize() > 0) {
                _recv_status = RECV_HTTP_ERROR;
                _resp_status = 400;
                return false;
            }
            return true;
        }
        if (line.size() > kMaxLine) {
            _recv_status = RECV_HTTP_ERROR;
            _resp_status = 414;
            return false;
        }
        bool ret = ParseHttpLine(line);
        if (ret == false) return false;
        _recv_status = RECV_HTTP_HEAD;
        return true;
    }

    bool RecvHttpHead(NetWork::Buffer* buf) {
        if (_recv_status != RECV_HTTP_HEAD) return false;
        while(1) {
            std::string line = buf->GetLineAndPop();
            if (line.size() == 0) {
                if (buf->ReadAbleSize() > kMaxLine) {
                    _recv_status = RECV_HTTP_ERROR;
                    _resp_status = 414;
                    return false;
                }
                if (buf->ReadAbleSize() > 0 || !_request._headers.empty()) {
                    _recv_status = RECV_HTTP_BODY;
                    return true;
                }
                return true;
            }
            if (line.size() > kMaxLine) {
                _recv_status = RECV_HTTP_ERROR;
                _resp_status = 414;
                return false;
            }
            if (line == "\r\n" || line == "\n") {
                _recv_status = RECV_HTTP_BODY;
                return true;
            }
            bool ret = ParseHttpHead(line);
            if (ret == false) return false;
        }
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
        std::string val = head.substr(pos + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        _request.SetHeader(key, val);
        return true;
    }

    bool RecvHttpBody(NetWork::Buffer* buf) {
        if (_recv_status != RECV_HTTP_BODY) return false;
        size_t content_length = _request.ContentLength();
        if (content_length == 0) {
            _recv_status = RECV_HTTP_OVER;
            return true;
        }
        size_t real_len = content_length - _request._body.size();
        if (buf->ReadAbleSize() >= real_len) {
            std::string body_data = buf->ReadAsStringAndPop(real_len);
            _request._body.append(body_data);
            _recv_status = RECV_HTTP_OVER;
            return true;
        }
        size_t readable = buf->ReadAbleSize();
        std::string body_data = buf->ReadAsStringAndPop(readable);
        _request._body.append(body_data);
        return true;
    }

private:
    int _resp_status;
    HttpRecvStatus _recv_status;
    HttpRequest _request;
};

class HttpCodec {
public:
    // 编码：HttpResponse → 完整 HTTP 响应报文
    static std::string Encode(const HttpResponse &rsp, const HttpRequest &req) {
        std::string header;
        header.reserve(256 + rsp._headers.size() * 64);
        header += req._version;
        header += ' ';
        header += std::to_string(rsp._status);
        header += ' ';
        header += Util::StatusDesc(rsp._status);
        header += "\r\n";
        for (auto &head : rsp._headers) {
            header += head.first;
            header += ": ";
            header += head.second;
            header += "\r\n";
        }
        header += "\r\n";
        return header + rsp._body;
    }
};