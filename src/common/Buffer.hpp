#pragma once

#include <vector>
#include <cstring>
#include <cassert>
#include <sys/uio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "Logger.hpp"
#include "noncopyable.hpp"

namespace NetWork
{

    static constexpr size_t kDefaultBufferSize = 1024;
    static constexpr size_t kCheapPrepend = 8;
    using std::vector;

class Buffer : public noncopyable {
    private:
        vector<char> _buffer; //使用vector进行内存空间管理
        uint64_t _read_idx; //读偏移
        uint64_t _write_idx; //写偏移
    public:
        Buffer():_buffer(kCheapPrepend + kDefaultBufferSize), _read_idx(kCheapPrepend), _write_idx(kCheapPrepend){}
        char *Begin() { return &*_buffer.begin(); }
        const char *Begin() const { return &*_buffer.begin(); }
        //获取当前写入起始地址, _buffer的空间起始地址，加上写偏移量
        char *WritePosition() { return Begin() + _write_idx; }
        //获取当前读取起始地址
        char *ReadPosition() { return Begin() + _read_idx; }
        const char *ReadPosition() const { return Begin() + _read_idx; }
        //获取缓冲区末尾空闲空间大小--写偏移之后的空闲空间, 总体空间大小减去写偏移
        uint64_t TailIdleSize() { return _buffer.size() - _write_idx; }
        //获取缓冲区起始空闲空间大小--读偏移之前的空闲空间
        uint64_t HeadIdleSize() const { return _read_idx; }
        //获取可读数据大小 = 写偏移 - 读偏移
        uint64_t ReadAbleSize() const { return _write_idx - _read_idx; }
        //获取可写数据大小 = 总体空间大小 - 写偏移
        uint64_t WritableSize() { return _buffer.size() - _write_idx; }
        //将读偏移向后移动
        void MoveReadOffset(uint64_t len) {
            if (len == 0) return;
            //向后移动的大小，必须小于可读数据大小
            assert(len <= ReadAbleSize());
            _read_idx += len;
        }
        //将写偏移向后移动
        void MoveWriteOffset(uint64_t len) {
            //向后移动的大小，必须小于当前后边的空闲空间大小
            assert(len <= TailIdleSize());
            _write_idx += len;
        }
        //确保可写空间足够（整体空闲空间够了就移动数据，否则就扩容）
        void EnsureWriteSpace(uint64_t len) {
            //如果末尾空闲空间大小足够，直接返回
            if (TailIdleSize() >= len) { return; }
            //末尾空闲空间不够，则判断加上起始位置的空闲空间大小是否足够, 够了就将数据移动到起始位置
            if (len <= TailIdleSize() + HeadIdleSize()) {
                //将数据移动到起始位置
                uint64_t rsz = ReadAbleSize();//把当前数据大小先保存起来
                std::copy(ReadPosition(), ReadPosition() + rsz, Begin());//把可读数据拷贝到起始位置
                _read_idx = 0;    //将读偏移归0
                _write_idx = rsz;  //将写位置置为可读数据大小， 因为当前的可读数据大小就是写偏移量
            }else {
                //总体空间不够，则需要扩容，不移动数据，直接给写偏移之后扩容足够空间即可
                LOG(DEBUG) << "RESIZE " << _write_idx + len;
                _buffer.resize(_write_idx + len);
            }
        }
        //写入数据
        void Write(const void *data, uint64_t len) {
            //1. 保证有足够空间，2. 拷贝数据进去
            if (len == 0) return;
            EnsureWriteSpace(len);
            const char *d = (const char *)data;
            std::copy(d, d + len, WritePosition());
        }
        void WriteAndPush(const void *data, uint64_t len) {
            Write(data, len);
            MoveWriteOffset(len);
        }
        void WriteString(const std::string &data) {
            return Write(data.c_str(), data.size());
        }
        void WriteStringAndPush(const std::string &data) {
            WriteString(data);
            MoveWriteOffset(data.size());
        }
        void WriteBuffer(Buffer &data) {
            return Write(data.ReadPosition(), data.ReadAbleSize());
        }
        void WriteBufferAndPush(Buffer &data) {
            WriteBuffer(data);
            MoveWriteOffset(data.ReadAbleSize());
        }
        //读取数据
        void Read(void *buf, uint64_t len) {
            //要求要获取的数据大小必须小于可读数据大小
            assert(len <= ReadAbleSize());
            std::copy(ReadPosition(), ReadPosition() + len, (char*)buf);
        }
        void ReadAndPop(void *buf, uint64_t len) {
            Read(buf, len);
            MoveReadOffset(len);
        }
        std::string ReadAsString(uint64_t len) {
            //要求要获取的数据大小必须小于可读数据大小
            assert(len <= ReadAbleSize());
            std::string str;
            str.resize(len);
            Read(&str[0], len);
            return str;
        }
        std::string ReadAsStringAndPop(size_t len) {
            if (ReadAbleSize() < len) return "";
            std::string str = ReadAsString(len);
            MoveReadOffset(len);
            return str;
        }

        char *FindLF() {
            char *res = (char*)memchr(ReadPosition(), '\n', ReadAbleSize());
            return res;
        }
        /*通常获取一行数据，这种情况针对是*/
        std::string GetLine() {
            char *pos = FindLF();
            if (pos == NULL) {
                return "";
            }
            // +1是为了把换行字符也取出来。
            return ReadAsString(pos - ReadPosition() + 1);
        }
        std::string GetLineAndPop() {
            std::string str = GetLine();
            MoveReadOffset(str.size());
            return str;
        }
        //清空缓冲区
        void Clear() {
            //只需要将偏移量归0即可（保留 prepend 空间）
            _read_idx = kCheapPrepend;
            _write_idx = kCheapPrepend;
            // 内存收缩优化：大流量后内存长期占用，当空闲空间超过阈值时收缩
            static constexpr size_t kShrinkThreshold = 64 * 1024;  // 64KB 阈值
            if (_buffer.size() > kDefaultBufferSize + kCheapPrepend + kShrinkThreshold) {
                _buffer.resize(kDefaultBufferSize + kCheapPrepend);
                _buffer.shrink_to_fit();
            }
        }

        // ===== Prepend 支持 =====
        // 获取 prependable 空间大小
        size_t PrependableSize() const { return _read_idx; }

        // 在前面追加数据（协议头封装零拷贝），返回是否成功
        bool Prepend(const void* data, size_t len) {
            if (len > PrependableSize()) return false;
            _read_idx -= len;
            const char* d = static_cast<const char*>(data);
            std::copy(d, d + len, Begin() + _read_idx);
            return true;
        }

        void PrependInt32(int32_t val) {
            int32_t be32 = htonl(val);
            Prepend(&be32, sizeof(be32));
        }

        // ===== 网络字节序支持 =====
        void AppendInt32(int32_t val) {
            int32_t be32 = htonl(val);
            WriteAndPush(&be32, sizeof(be32));
        }

        int32_t ReadInt32() {
            int32_t be32 = 0;
            ReadAndPop(&be32, sizeof(be32));
            return ntohl(be32);
        }

        int32_t PeekInt32() const {
            int32_t be32 = 0;
            ::memcpy(&be32, ReadPosition(), sizeof(be32));
            return ntohl(be32);
        }

        // 使用 readv 从 fd 读取数据，参考 muduo 原版 Buffer::readFd()
        // 第一块写入 Buffer 可写区，第二块写入栈上 extrabuf，按需追加
        ssize_t ReadFd(int fd) {
            char extrabuf[65536];  // 栈上溢出缓冲区
            struct iovec vec[2];
            const size_t writable = WritableSize();
            vec[0].iov_base = WritePosition();
            vec[0].iov_len = writable;
            vec[1].iov_base = extrabuf;
            vec[1].iov_len = sizeof(extrabuf);

            ssize_t n = ::readv(fd, vec, 2);
            if (n < 0) return n;
            if ((size_t)n <= writable) {
                // 数据全部在 Buffer 中，无需额外拷贝
                MoveWriteOffset(n);
            } else {
                // extrabuf 中也有数据，追加到 Buffer
                MoveWriteOffset(writable);
                WriteAndPush(extrabuf, n - writable);
            }
            return n;
        }

        // 使用 write 从 Buffer 直接写出数据到 fd
        ssize_t WriteFd(int fd) {
            ssize_t n = ::write(fd, ReadPosition(), ReadAbleSize());
            if (n > 0) {
                MoveReadOffset(n);
            }
            return n;
        }
};




}
