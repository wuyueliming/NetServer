#pragma once

#include <atomic>
#include <memory>
#include <functional>
#include <string>
#include <any>  // C++17 std::any，用于用户自定义状态存储
#include "Buffer.hpp"
#include "InetAddr.hpp"

namespace NetWork {
    class Connection;
    class EventLoop;

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
    using ServerCallback = std::function<void(ConnectionID, EventLoop*)>;

    // Connection 是会话层连接的抽象接口
    // 上层通过 ConnectionPtr 操作，不感知底层是 TCP 还是 UDP
    // 数据流：socket → _inBuffer → OnMessage 回调 → 应用层通过 ReadBuffer() 直接操作
    class Connection : public std::enable_shared_from_this<Connection> {
    public:
        virtual ~Connection() = default;

        // ===== IO 接口 =====
        virtual void Send(const void *data, size_t len) = 0;
        virtual void Send(std::string &&msg) = 0;  // move 语义，减少拷贝
        // 直接访问输入缓冲区（应用层自行处理粘包）
        virtual Buffer* ReadBuffer() = 0;
        virtual size_t ReadableSize() const = 0;

        // ===== 连接管理 =====
        virtual void Shutdown() = 0;
        virtual void Release() = 0;
        virtual void EnableInactiveRelease(uint32_t timeout) = 0;
        virtual void DisableInactiveRelease() = 0;

        // ===== Server 内部 setup 方法 =====
        virtual void Established() = 0;
        virtual void SetConnectedCallback(const AppLayerCallback& cb) = 0;
        virtual void SetMessageCallback(const AppLayerCallback& cb) = 0;
        virtual void SetClosedCallback(const AppLayerCallback& cb) = 0;
        virtual void SetEventCallback(const AppLayerCallback& cb) = 0;
        virtual void SetReleaseCallback(const ServerCallback& cb) = 0;
        virtual void SetWriteCompleteCallback(const AppLayerCallback& cb) = 0;

        // 非虚方法——所有 Connection 共有
        InetAddr PeerAddr() const { return _peer; }
        ConnectionID ID() const { return _id; }
        bool isConnected() const { return static_cast<ConnectionState>(_state.load()) == ConnectionState::CONNECTED; }
        bool isDisconnected() const { return static_cast<ConnectionState>(_state.load()) == ConnectionState::DISCONNECTED; }

        // 连接迁移相关（TCP 专属，UDP 返回 nullptr）
        virtual void MigrateTo(EventLoop *new_loop) {}
        virtual EventLoop* getLoop() const { return nullptr; }

        // ===== 用户自定义状态存储（context）=====
        void setContext(const std::any& context) { _context = context; }
        void setContext(std::any&& context) { _context = std::move(context); }  // move 版本
        const std::any& getContext() const { return _context; }
        std::any* getMutableContext() { return &_context; }

        // ===== 协议升级 =====
        // 用于协议升级（如 HTTP → WebSocket），同时更新 context 和回调函数
        // 必须在 EventLoop 线程中调用，保证原子性，避免新事件触发时使用旧协议处理
        virtual void Upgrade(const std::any& context,
                             const AppLayerCallback& connected_cb,
                             const AppLayerCallback& message_cb,
                             const AppLayerCallback& closed_cb,
                             const AppLayerCallback& event_cb) = 0;

    protected:
        Connection(ConnectionID id, InetAddr addr)
            : _peer(addr), _id(id), _state(static_cast<int>(ConnectionState::CONNECTING)) {}

        const InetAddr _peer;
        const ConnectionID _id;
        std::atomic<int> _state;  // 存储 ConnectionState 的原子变量
        Buffer _inBuffer;
        std::any _context;  // 用户自定义状态存储
        AppLayerCallback _onConnected_Callback;
        AppLayerCallback _onMessage_Callback;
        AppLayerCallback _onClosed_Callback;
        AppLayerCallback _onEvent_Callback;
    };

}