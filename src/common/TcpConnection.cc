#include "TcpConnection.h"
#include "WeakCallback.hpp"
#include <csignal>

namespace NetWork {

    // 初始化时屏蔽 SIGPIPE（防止 write/writev 对已关闭连接触发 SIGPIPE）
    static struct SigPipeInit {
        SigPipeInit() { ::signal(SIGPIPE, SIG_IGN); }
    } sigPipeInit;

    TcpConnection::TcpConnection(EventLoop *loop, ConnectionID id, int fd, InetAddr addr)
        : Connection(id, addr),
          _ioSock(fd), _loop(loop), _channel(loop, fd),
          _enable_inactive_release(false), _timeout(0), _releaseTaskId(INVALID_TASK_ID)
    {
        // ET模式设置非阻塞
        _ioSock.SetNonBlock();
        _channel.SetReadCallback([this]() { OnRead(); });
        _channel.SetWriteCallback([this]() { OnWrite(); });
        _channel.SetCloseCallback([this]() { OnClose(); });
        _channel.SetErrorCallback([this]() { OnError(); });
        _channel.SetEventCallback([this]() { OnEvent(); });
        LOG(INFO) << "TcpConnection Created, connID:" << _id << " fd:" << _ioSock.Fd();
    }

    TcpConnection::~TcpConnection() {
        LOG(INFO) << "TcpConnection Destroyed, connID:" << _id;
    }

    // Channel事件回调 - 使用 readv 零拷贝读取
    void TcpConnection::OnRead() {
        if (static_cast<ConnectionState>(_state.load()) == ConnectionState::DISCONNECTED ||
            static_cast<ConnectionState>(_state.load()) == ConnectionState::DISCONNECTING) return;
        while (true) {
            ssize_t ret = _inBuffer.ReadFd(_ioSock.Fd());
            if (ret > 0) {
                // 数据已直接写入 _inBuffer，无需额外 memcpy
            } else if (ret == 0) {
                // 对端关闭连接
                if (_inBuffer.ReadAbleSize() > 0) {
                    if (_onMessage_Callback) {
                        _onMessage_Callback(shared_from_this());
                    }
                }
                // 回调可能改变状态，重新检查
                if (static_cast<ConnectionState>(_state.load()) == ConnectionState::DISCONNECTED ||
                    static_cast<ConnectionState>(_state.load()) == ConnectionState::DISCONNECTING) return;
                // 如果输出缓冲区有数据，先发完再关闭
                if (_outBuffer.ReadAbleSize() > 0) {
                    _state.store(static_cast<int>(ConnectionState::DISCONNECTING));
                    if (!_channel.WriteAble()) {
                        _channel.EnableWriteET();
                    }
                    return;
                }
                return Release();
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 非阻塞模式正常返回：数据已读完
                    break;
                }
                if (errno == EINTR) {
                    // 被信号中断，继续读取
                    continue;
                }
                // 真正的错误
                return Release();
            }
        }
        // 直接调用回调，不切帧
        if (_inBuffer.ReadAbleSize() > 0) {
            if (_onMessage_Callback) {
                _onMessage_Callback(shared_from_this());
            }
            // 回调可能改变状态，重新检查
            if (static_cast<ConnectionState>(_state.load()) == ConnectionState::DISCONNECTED ||
                static_cast<ConnectionState>(_state.load()) == ConnectionState::DISCONNECTING) return;
        }
    }

    void TcpConnection::OnWrite() {
        // ET模式必须循环发送，直到数据发完或EAGAIN
        while (_outBuffer.ReadAbleSize() > 0) {
            ssize_t ret = _outBuffer.WriteFd(_ioSock.Fd());
            if (ret > 0) {
                // 数据已直接从 _outBuffer 写出
            } else if (ret < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 缓冲区满了，下次再发
                    break;
                }
                if (errno == EINTR) {
                    // 被信号中断，继续发送
                    continue;
                }
                // 真正的错误
                return Release();
            }
        }
        // 数据发完了，关闭写监控
        if (_outBuffer.ReadAbleSize() == 0) {
            _channel.DisableWrite();
            // 写完成回调：输出缓冲区已清空
            if (_writeCompleteCallback) {
                _writeCompleteCallback(shared_from_this());
            }
            // 如果状态是DISCONNECTING，说明应用层调用了Shutdown，等数据发完再关闭
            if (static_cast<ConnectionState>(_state.load()) == ConnectionState::DISCONNECTING) {
                return Release();
            }
        }
    }

    void TcpConnection::OnClose() {
        /*一旦连接挂断了，套接字就什么都干不了了，因此有数据待处理就处理一下，完毕关闭连接*/
        if (_inBuffer.ReadAbleSize() > 0) {
            if (_onMessage_Callback) {
                _onMessage_Callback(shared_from_this());
            }
        }
        if (static_cast<ConnectionState>(_state.load()) == ConnectionState::DISCONNECTED ||
            static_cast<ConnectionState>(_state.load()) == ConnectionState::DISCONNECTING) return;
        // 如果输出缓冲区有数据，先发完再关闭
        if (_outBuffer.ReadAbleSize() > 0) {
            _state.store(static_cast<int>(ConnectionState::DISCONNECTING));
            if (!_channel.WriteAble()) {
                _channel.EnableWriteET();
            }
            return;
        }
        return Release();
    }

    void TcpConnection::OnError() {
        return OnClose();
    }

    void TcpConnection::OnEvent() {
        // 有可能调用OnEvent之前连接断开了，就不需要刷新定时任务了
        if (_enable_inactive_release == true && static_cast<ConnectionState>(_state.load()) != ConnectionState::DISCONNECTED) {
            _loop->RefreshTimedTask(_releaseTaskId, _timeout);
        }
        if (_onEvent_Callback) {
            _onEvent_Callback(shared_from_this());
        }
    }

    // Connection接口实现 - 使用 lambda + move 替代 std::bind
    void TcpConnection::Established() {
        _loop->runInLoop([this]() { EstablishedInLoop(); });
    }

    void TcpConnection::Shutdown() {
        _loop->runInLoop([this]() { ShutdownInLoop(); });
    }

    void TcpConnection::Release() {
        // 捕获 shared_ptr 而非裸指针, 防止任务执行前 TcpConnection 被析构 (use-after-free)
        _loop->runInLoop([self = std::static_pointer_cast<TcpConnection>(shared_from_this())]() { self->ReleaseInLoop(); });
    }

    void TcpConnection::Send(const void *data, size_t len) {
        if (data == nullptr || len == 0) return;
        if (_loop->isInLoopThread()) {
            // IO线程内：零拷贝，直接写入
            SendInLoop(data, len);
        } else {
            // 跨线程：必须拷贝数据（指针可能失效）
            std::string msg((const char*)data, len);
            _loop->runInLoop([this, msg = std::move(msg)]() {
                SendInLoop(msg.data(), msg.size());
            });
        }
    }

    void TcpConnection::Send(std::string &&msg) {
        if (msg.empty()) return;
        if (_loop->isInLoopThread()) {
            SendInLoop(msg.data(), msg.size());
        } else {
            // 跨线程：move 进 lambda，省一次拷贝
            _loop->runInLoop([this, msg = std::move(msg)]() {
                SendInLoop(msg.data(), msg.size());
            });
        }
    }

    void TcpConnection::EnableInactiveRelease(uint32_t timeout) {
        _loop->runInLoop([this, timeout]() { EnableInactiveReleaseInLoop(timeout); });
    }

    void TcpConnection::DisableInactiveRelease() {
        _loop->runInLoop([this]() { DisableInactiveReleaseInLoop(); });
    }

    // InLoop方法
    void TcpConnection::EstablishedInLoop() {
        _state.store(static_cast<int>(ConnectionState::CONNECTED));
        // 绑定 Channel 生命周期到 Connection，防止事件处理中连接被销毁
        _channel.Tie(shared_from_this());
        if (_onConnected_Callback) {
            _onConnected_Callback(shared_from_this());
        }
        _channel.EnableReadET();
    }

    void TcpConnection::ShutdownInLoop() {
        _state.store(static_cast<int>(ConnectionState::DISCONNECTING));
        // 处理输入缓冲区剩余数据
        if (_inBuffer.ReadAbleSize() > 0) {
            if (_onMessage_Callback) {
                _onMessage_Callback(shared_from_this());
            }
        }
        // 有数据要发，启用写监控（发完后 OnWrite 会调用 Release）
        if (_outBuffer.ReadAbleSize() > 0) {
            if (_channel.WriteAble() == false) {
                _channel.EnableWriteET();
            }
            return;  // 等 OnWrite 发完
        }
        // 输入输出都处理完了，直接关闭
        Release();
    }

    void TcpConnection::ReleaseInLoop() {
        // 持有 self 引用，防止回调中释放最后一个 shared_ptr 导致 use-after-free
        auto self = shared_from_this();
        // 1. 修改连接状态，将其置为DISCONNECTED
        if (static_cast<ConnectionState>(_state.load()) == ConnectionState::DISCONNECTED) return;
        _state.store(static_cast<int>(ConnectionState::DISCONNECTED));
        // 2. 从 epoll 中移除监控
        _channel.Remove();
        // 3. 不手动 Close socket，依赖 TcpSocket 析构自动 Close
        //    这样可以确保在回调函数执行期间 socket 仍然有效
        // 4. 如果当前定时器队列中还有定时销毁任务，则取消任务
        if (_enable_inactive_release) {
            _loop->CancelTimedTask(_releaseTaskId);
            _enable_inactive_release = false;
        }
        // 5. 调用关闭回调函数
        if (_onClosed_Callback) _onClosed_Callback(self);
        // 移除服务器内部管理的连接信息
        if (_release_Callback) _release_Callback(_id, _loop);
    }

    void TcpConnection::SendInLoop(const void *data, size_t len) {
        if (static_cast<ConnectionState>(_state.load()) == ConnectionState::DISCONNECTED) return;

        if (_outBuffer.ReadAbleSize() == 0 && !_channel.WriteAble()) {
            // 快路径：输出缓冲区为空，尝试直接写
            ssize_t nwrote = ::write(_ioSock.Fd(), data, len);
            if (nwrote >= 0) {
                size_t remaining = len - nwrote;
                if (remaining == 0) {
                    if (_writeCompleteCallback) _writeCompleteCallback(shared_from_this());
                    return;
                }
                // 写不完，剩余数据放入 _outBuffer
                _outBuffer.WriteAndPush((const char*)data + nwrote, remaining);
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                LOG(ERROR) << "SendInLoop write error: " << std::strerror(errno);
                return Release();
            } else {
                // EAGAIN，全部数据放入 _outBuffer
                _outBuffer.WriteAndPush(data, len);
            }
        } else if (_outBuffer.ReadAbleSize() > 0) {
            // 慢路径：缓冲区已有数据，用 writev 一次写出两块
            struct iovec vec[2];
            vec[0].iov_base = _outBuffer.ReadPosition();
            vec[0].iov_len = _outBuffer.ReadAbleSize();
            vec[1].iov_base = (void*)data;  // writev 要求非 const 指针，此处强转安全
            vec[1].iov_len = len;

            ssize_t n = ::writev(_ioSock.Fd(), vec, 2);
            if (n >= 0) {
                size_t buf_len = _outBuffer.ReadAbleSize();
                if ((size_t)n <= buf_len) {
                    // 只消费了 _outBuffer 的部分
                    _outBuffer.MoveReadOffset(n);
                    // 新数据全部放入 _outBuffer
                    _outBuffer.WriteAndPush(data, len);
                } else {
                    // _outBuffer 全部写出 + 新数据部分写出
                    size_t data_written = n - buf_len;
                    _outBuffer.Clear();  // _outBuffer 已全部写出
                    size_t data_remaining = len - data_written;
                    if (data_remaining > 0) {
                        _outBuffer.WriteAndPush((const char*)data + data_written, data_remaining);
                    } else {
                        // 全部写完
                        if (_writeCompleteCallback) _writeCompleteCallback(shared_from_this());
                        return;
                    }
                }
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                LOG(ERROR) << "SendInLoop writev error: " << std::strerror(errno);
                return Release();
            } else {
                // EAGAIN，新数据追加到 _outBuffer
                _outBuffer.WriteAndPush(data, len);
            }
        } else {
            // _outBuffer 为空但注册了可写事件（理论上不应出现，保险处理）
            _outBuffer.WriteAndPush(data, len);
        }

        if (!_channel.WriteAble()) {
            _channel.EnableWriteET();
        }
    }

    void TcpConnection::EnableInactiveReleaseInLoop(uint32_t timeout) {
        _enable_inactive_release = true;
        _timeout = timeout;
        if (_loop->existTask(_releaseTaskId)) {
            _loop->RefreshTimedTask(_releaseTaskId, _timeout);
        } else {
            _loop->AddTimedTask(_timeout, MakeWeakCallback(std::static_pointer_cast<TcpConnection>(shared_from_this()), &TcpConnection::ReleaseInLoop), &_releaseTaskId);
        }
    }

    void TcpConnection::DisableInactiveReleaseInLoop() {
        _enable_inactive_release = false;
        _loop->CancelTimedTask(_releaseTaskId);
    }

    void TcpConnection::MigrateTo(EventLoop *new_loop) {
        _loop->runInLoop([this, new_loop]() { MigrateToInLoop(new_loop); });
    }

    void TcpConnection::MigrateToInLoop(EventLoop *new_loop) {
        if (_loop == new_loop) return;  // 无需迁移
        // 1. 取消旧定时器
        if (_enable_inactive_release && _loop->existTask(_releaseTaskId)) {
            _loop->CancelTimedTask(_releaseTaskId);
        }
        // 2. 先从旧 epoll 移除（必须在 SetLoop 之前，否则 Remove 会操作错误的 epoll 实例）
        _channel.Remove();
        // 3. 切换 loop
        _loop = new_loop;
        _channel.SetLoop(new_loop);
        // 4. 在新 loop 中重新注册
        new_loop->runInLoop([this]() {
            _channel.EnableReadET();
            // 5. 重新注册定时器
            if (_enable_inactive_release) {
                _loop->AddTimedTask(_timeout, MakeWeakCallback(std::static_pointer_cast<TcpConnection>(shared_from_this()), &TcpConnection::ReleaseInLoop), &_releaseTaskId);
            }
        });
    }

    // 协议升级：同时更新 context 和回调函数，保证原子性
    void TcpConnection::Upgrade(const std::any& context,
                                const AppLayerCallback& connected_cb,
                                const AppLayerCallback& message_cb,
                                const AppLayerCallback& closed_cb,
                                const AppLayerCallback& event_cb) {
        _loop->runInLoop([this, context, connected_cb, message_cb, closed_cb, event_cb]() {
            UpgradeInLoop(context, connected_cb, message_cb, closed_cb, event_cb);
        });
    }

    void TcpConnection::UpgradeInLoop(const std::any& context,
                                      const AppLayerCallback& connected_cb,
                                      const AppLayerCallback& message_cb,
                                      const AppLayerCallback& closed_cb,
                                      const AppLayerCallback& event_cb) {
        _context = context;
        _onConnected_Callback = connected_cb;
        _onMessage_Callback = message_cb;
        _onClosed_Callback = closed_cb;
        _onEvent_Callback = event_cb;
    }

}
