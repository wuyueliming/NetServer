#pragma once

#include "Connection.h"
#include "TimeWheel.h"
#include "base/TcpSocket.hpp"
#include "Channel.h"
#include "base/Logger.hpp"
#include "Reactor.h"
#include <queue>
#include <cassert>

namespace Aether {

    class TcpConnection : public Connection {
    public:
        TcpConnection(Reactor *loop, ConnectionID id, int fd, InetAddr addr);
        ~TcpConnection();

        // ===== IO 接口 =====
        void Send(const void *data, size_t len) override;
        void Send(std::string &&msg) override;
        std::string Recv() override;
        bool HasMessage() override;

        // ===== 连接管理 =====
        void Shutdown() override;
        void Release() override;
        void EnableInactiveRelease(uint32_t timeout) override;
        void DisableInactiveRelease() override;
        void SetFrameDecoder(std::unique_ptr<FrameDecoder> decoder) override;

        // ===== Server 内部 setup 方法 =====
        void Established() override;
        void SetConnectedCallback(const AppLayerCallback& cb) override { _onConnected_Callback = cb; }
        void SetMessageCallback(const AppLayerCallback& cb) override { _onMessage_Callback = cb; }
        void SetClosedCallback(const AppLayerCallback& cb) override { _onClosed_Callback = cb; }
        void SetEventCallback(const AppLayerCallback& cb) override { _onEvent_Callback = cb; }
        void SetReleaseCallback(const ServerCallback& cb) override { _release_Callback = cb; }
        void SetHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t mark) override {
            _highWaterMark = mark;
            _highWaterMarkCallback = cb;
        }
        void SetWriteCompleteCallback(const WriteCompleteCallback& cb) override {
            _writeCompleteCallback = cb;
        }

        // 连接迁移
        void MigrateTo(Reactor *new_loop) override;
        Reactor* GetLoop() const override { return _loop; }

    private:
        void OnRead();
        void OnWrite();
        void OnError();
        void OnClose();
        void OnEvent();

        void EstablishedInLoop();
        void ShutdownInLoop();
        void ReleaseInLoop();
        void SendInLoop(const void *data, size_t len);
        void EnableInactiveReleaseInLoop(uint32_t timeout);
        void DisableInactiveReleaseInLoop();
        void FrameDecodeInLoop();
        void MigrateToInLoop(Reactor *new_loop);

    private:
        TcpSocket _ioSock;
        Reactor *_loop;
        Channel _channel;
        Buffer _outBuffer;
        std::unique_ptr<FrameDecoder> _frame_decoder;  // 帧解码器（每个连接独占）
        std::queue<std::string> _frames;           // 解析出的完整报文帧队列
        bool _enable_inactive_release;
        uint32_t _timeout;
        TaskID _releaseTaskId;
        ServerCallback _release_Callback;
        size_t _highWaterMark = 64 * 1024 * 1024;  // 默认 64MB
        HighWaterMarkCallback _highWaterMarkCallback;
        WriteCompleteCallback _writeCompleteCallback;
    };

}
