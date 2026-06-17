#pragma once

#include <string>
#include <vector>
#include <regex>
#include "Util.hpp"
#include "httpRequest.hpp"
#include "httpResponse.hpp"
#include <Aether/FrameDecoder.hpp>




class HttpFrameDecoder : public Aether::FrameDecoder {
public:
    HttpFrameDecoder() : _resp_status(200), _recv_status(RECV_HTTP_LINE) {}

    bool TryDecode(Aether::Buffer &buffer, std::string &out_frame) override {
        while (buffer.ReadAbleSize() > 0 && _resp_status < 400) {
            switch(_recv_status) {
                case RECV_HTTP_LINE:
                    if (!RecvHttpLine(buffer)) break;
                    [[fallthrough]];
                case RECV_HTTP_HEAD:
                    if (!RecvHttpHead(buffer)) break;
                    [[fallthrough]];
                case RECV_HTTP_BODY:
                    RecvHttpBody(buffer);
                    break;
                default: break;
            }
            if (_recv_status == RECV_HTTP_OVER) {
                out_frame = SerializeRequest(_request);
                Reset();
                return true;
            } else {
                break;
            }
        }
        if (_resp_status >= 400) {
            buffer.MoveReadOffset(buffer.ReadAbleSize());
            out_frame.clear();
            int status = _resp_status;
            Reset();
            _resp_status = status;
            return true;
        }
        return false;
    }

    void Reset() override {
        _resp_status = 200;
        _recv_status = RECV_HTTP_LINE;
        _request.ReSet();
    }

    // 获取错误状态码（HttpServer 需要读取）
    int ErrorStatus() const { return _resp_status; }

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

    bool RecvHttpLine(Aether::Buffer &buffer) {
        if (_recv_status != RECV_HTTP_LINE) return false;
        std::string line = buffer.GetLineAndPop();
        if (line.size() == 0) {
            if (buffer.ReadAbleSize() > kMaxLine) {
                _recv_status = RECV_HTTP_ERROR;
                _resp_status = 414;
                return false;
            }
            if (buffer.ReadAbleSize() > 0) {
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

    bool RecvHttpHead(Aether::Buffer &buffer) {
        if (_recv_status != RECV_HTTP_HEAD) return false;
        while(1) {
            std::string line = buffer.GetLineAndPop();
            if (line.size() == 0) {
                if (buffer.ReadAbleSize() > kMaxLine) {
                    _recv_status = RECV_HTTP_ERROR;
                    _resp_status = 414;
                    return false;
                }
                if (buffer.ReadAbleSize() > 0 || !_request._headers.empty()) {
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

    bool RecvHttpBody(Aether::Buffer &buffer) {
        if (_recv_status != RECV_HTTP_BODY) return false;
        size_t content_length = _request.ContentLength();
        if (content_length == 0) {
            _recv_status = RECV_HTTP_OVER;
            return true;
        }
        size_t real_len = content_length - _request._body.size();
        if (buffer.ReadAbleSize() >= real_len) {
            std::string body_data = buffer.ReadAsStringAndPop(real_len);
            _request._body.append(body_data);
            _recv_status = RECV_HTTP_OVER;
            return true;
        }
        size_t readable = buffer.ReadAbleSize();
        std::string body_data = buffer.ReadAsStringAndPop(readable);
        _request._body.append(body_data);
        return true;
    }

    // 将解析好的 HttpRequest 序列化为字符串帧
    // 这样 HttpCodec 可以从字符串帧中反序列化
    static std::string SerializeRequest(const HttpRequest &req) {
        std::string frame;
        frame += req._method + " " + req._path;
        if (!req._params.empty()) {
            frame += "?";
            bool first = true;
            for (auto &p : req._params) {
                if (!first) frame += "&";
                frame += Util::UrlEncode(p.first, true) + "=" + Util::UrlEncode(p.second, true);
                first = false;
            }
        }
        frame += " " + req._version + "\r\n";
        for (auto &h : req._headers) {
            frame += h.first + ": " + h.second + "\r\n";
        }
        frame += "\r\n";
        frame += req._body;
        return frame;
    }

private:
    int _resp_status;
    HttpRecvStatus _recv_status;
    HttpRequest _request;
};

class HttpCodec {
public:
    // 解码：完整 HTTP 报文帧 → HttpRequest
    // 注意：HttpFrameDecoder 已经完成了粘包处理和解析，
    //       这里直接从帧中重建 HttpRequest 对象
    //       由于 HttpFrameDecoder 内部已经持有解析好的 HttpRequest，
    //       实际上帧的格式是我们自定义的序列化格式
    static HttpRequest Decode(const std::string &frame) {
        HttpRequest req;
        if (frame.empty()) return req;

        size_t pos = 0;
        // 解析请求行
        size_t line_end = frame.find("\r\n");
        if (line_end == std::string::npos) return req;

        std::string request_line = frame.substr(0, line_end);
        pos = line_end + 2;

        // 解析 method path version
        size_t sp1 = request_line.find(' ');
        if (sp1 == std::string::npos) return req;
        size_t sp2 = request_line.find(' ', sp1 + 1);
        if (sp2 == std::string::npos) return req;

        req._method = request_line.substr(0, sp1);
        std::string full_path = request_line.substr(sp1 + 1, sp2 - sp1 - 1);
        req._version = request_line.substr(sp2 + 1);

        // 解析 path 和 query string
        size_t qmark = full_path.find('?');
        if (qmark != std::string::npos) {
            req._path = Util::UrlDecode(full_path.substr(0, qmark), false);
            std::string query = full_path.substr(qmark + 1);
            std::vector<std::string> params;
            Util::Split(query, "&", &params);
            for (auto &str : params) {
                size_t eq = str.find('=');
                if (eq == std::string::npos) {
                    req.SetParam(Util::UrlDecode(str, true), "");
                } else {
                    req.SetParam(Util::UrlDecode(str.substr(0, eq), true),
                                 Util::UrlDecode(str.substr(eq + 1), true));
                }
            }
        } else {
            req._path = Util::UrlDecode(full_path, false);
        }

        // 解析头部
        while (pos < frame.size()) {
            line_end = frame.find("\r\n", pos);
            if (line_end == std::string::npos) break;
            if (line_end == pos) {
                pos += 2;
                break;  // 空行，头部结束
            }
            std::string header_line = frame.substr(pos, line_end - pos);
            pos = line_end + 2;
            size_t colon = header_line.find(':');
            if (colon == std::string::npos) continue;
            std::string key = header_line.substr(0, colon);
            std::string val = header_line.substr(colon + 1);
            val.erase(0, val.find_first_not_of(" \t"));
            req.SetHeader(key, val);
        }

        // 剩余部分是 body
        if (pos < frame.size()) {
            req._body = frame.substr(pos);
        }

        return req;
    }

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
