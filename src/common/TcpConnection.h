#pragma once

#include "Connection.h"
#include "TimeWheel.h"
#include "TcpSocket.hpp"
#include "Channel.h"
#include "Logger.hpp"
#include "EventLoop.h"

namespace NetWork {

    class TcpConnection : public Connection {
    public:
        TcpConnection(EventLoop *loop, ConnectionID id, int fd, InetAddr addr);
        ~TcpConnection();

        // ===== IO 接口 =====
        void Send(const void *data, size_t len) override;
        void Send(std::string &&msg) override;
        Buffer* ReadBuffer() override { return &_inBuffer; }
        size_t ReadableSize() const override { return _inBuffer.ReadAbleSize(); }

        // ===== 连接管理 =====
        void Shutdown() override;
        void Release() override;
        void EnableInactiveRelease(uint32_t timeout) override;
        void DisableInactiveRelease() override;

        // ===== Server 内部 setup 方法 =====
        void Established() override;
        void SetConnectedCallback(const AppLayerCallback& cb) override { _onConnected_Callback = cb; }
        void SetMessageCallback(const AppLayerCallback& cb) override { _onMessage_Callback = cb; }
        void SetClosedCallback(const AppLayerCallback& cb) override { _onClosed_Callback = cb; }
        void SetEventCallback(const AppLayerCallback& cb) override { _onEvent_Callback = cb; }
        void SetReleaseCallback(const ServerCallback& cb) override { _release_Callback = cb; }
        void SetWriteCompleteCallback(const AppLayerCallback& cb) override {
            _writeCompleteCallback = cb;
        }

        // 连接迁移
        void MigrateTo(EventLoop *new_loop) override;
        EventLoop* getLoop() const override { return _loop; }

        // 协议升级
        void Upgrade(const std::any& context,
                     const AppLayerCallback& connected_cb,
                     const AppLayerCallback& message_cb,
                     const AppLayerCallback& closed_cb,
                     const AppLayerCallback& event_cb) override;

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
        void MigrateToInLoop(EventLoop *new_loop);
        void UpgradeInLoop(const std::any& context,
                           const AppLayerCallback& connected_cb,
                           const AppLayerCallback& message_cb,
                           const AppLayerCallback& closed_cb,
                           const AppLayerCallback& event_cb);

    private:
        TcpSocket _ioSock;
        EventLoop *_loop;
        Channel _channel;
        Buffer _outBuffer;
        bool _enable_inactive_release;
        uint32_t _timeout;
        TaskID _releaseTaskId;
        ServerCallback _release_Callback;
        AppLayerCallback _writeCompleteCallback;
    };

}