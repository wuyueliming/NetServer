#pragma once

#include <atomic>
#include <memory>
#include <functional>
#include <string>
#include "base/Buffer.hpp"
#include "base/InetAddr.hpp"
#include "FrameDecoder.hpp"

namespace Aether {
    class Connection;
    class Reactor;

    // 连接状态
    enum class ConnectionState {
        DISCONNECTED,
        CONNECTED,
        CONNECTING,
        DISCONNECTING,
    };

    using ConnectionID = uint64_t;
    using ConnectionPtr = std::shared_ptr<Connection>;
    using ConnectionWeakPtr = std::weak_ptr<Connection>;
    using AppLayerCallback = std::function<void(ConnectionPtr)>;
    using HighWaterMarkCallback = std::function<void(ConnectionPtr, size_t)>;
    using WriteCompleteCallback = std::function<void(ConnectionPtr)>;
    using ServerCallback = std::function<void(ConnectionID, Reactor*)>;
    using FrameDecoderFactory = std::function<std::unique_ptr<FrameDecoder>()>;

    // Connection 是会话层连接的抽象接口
    // 上层通过 ConnectionPtr 操作，不感知底层是 TCP 还是 UDP
    // IO 接口只有 Recv（取完整帧）和 Send（发送字节流）
    class Connection : public std::enable_shared_from_this<Connection> {
    public:
        virtual ~Connection() = default;

        // ===== IO 接口 =====
        virtual void Send(const void *data, size_t len) = 0;
        virtual void Send(std::string &&msg) = 0;  // move 语义，减少拷贝
        // 取一个完整报文帧（由 FrameDecoder 切分，不含分隔符）
        virtual std::string Recv() = 0;
        // 是否有完整帧可取
        virtual bool HasMessage() = 0;

        // ===== 连接管理 =====
        virtual void Shutdown() = 0;
        virtual void Release() = 0;
        virtual void EnableInactiveRelease(uint32_t timeout) = 0;
        virtual void DisableInactiveRelease() = 0;
        virtual void SetFrameDecoder(std::unique_ptr<FrameDecoder> decoder) = 0;

        // ===== Server 内部 setup 方法 =====
        virtual void Established() = 0;
        virtual void SetConnectedCallback(const AppLayerCallback& cb) = 0;
        virtual void SetMessageCallback(const AppLayerCallback& cb) = 0;
        virtual void SetClosedCallback(const AppLayerCallback& cb) = 0;
        virtual void SetEventCallback(const AppLayerCallback& cb) = 0;
        virtual void SetReleaseCallback(const ServerCallback& cb) = 0;
        virtual void SetHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t mark) = 0;
        virtual void SetWriteCompleteCallback(const WriteCompleteCallback& cb) = 0;

        // 非虚方法——所有 Connection 共有
        InetAddr PeerAddr() const { return _peer; }
        ConnectionID ID() const { return _id; }
        bool isConnected() const { return static_cast<ConnectionState>(_state.load()) == ConnectionState::CONNECTED; }
        bool isDisconnected() const { return static_cast<ConnectionState>(_state.load()) == ConnectionState::DISCONNECTED; }

        // 连接迁移相关（TCP 专属，UDP 返回 nullptr）
        virtual void MigrateTo(Reactor *new_loop) {}
        virtual Reactor* GetLoop() const { return nullptr; }

    protected:
        Connection(ConnectionID id, InetAddr addr)
            : _peer(addr), _id(id), _state(static_cast<int>(ConnectionState::CONNECTING)) {}

        const InetAddr _peer;
        const ConnectionID _id;
        std::atomic<int> _state;  // 存储 ConnectionState 的原子变量
        Buffer _inBuffer;
        AppLayerCallback _onConnected_Callback;
        AppLayerCallback _onMessage_Callback;
        AppLayerCallback _onClosed_Callback;
        AppLayerCallback _onEvent_Callback;
    };

}
