#pragma once

#include <memory>
#include <functional>

namespace Aether {

// WeakCallback：用于定时器回调中避免延长对象生命周期
// 设计理念：持有 weak_ptr，回调时尝试提升为 shared_ptr
// 如果对象已销毁，回调静默跳过，避免访问已析构对象
template<typename CLASS, typename... ARGS>
class WeakCallback {
public:
    WeakCallback(const std::weak_ptr<CLASS>& object,
                 const std::function<void(CLASS*, ARGS...)>& function)
        : _object(object), _function(function) {}

    void operator()(ARGS&&... args) const {
        std::shared_ptr<CLASS> ptr(_object.lock());
        if (ptr) {
            _function(ptr.get(), std::forward<ARGS>(args)...);
        }
        // 如果对象已销毁，回调静默跳过
    }

private:
    std::weak_ptr<CLASS> _object;
    std::function<void(CLASS*, ARGS...)> _function;
};

// 辅助函数：创建 WeakCallback
template<typename CLASS, typename... ARGS>
WeakCallback<CLASS, ARGS...> MakeWeakCallback(const std::shared_ptr<CLASS>& object,
                                               void (CLASS::*method)(ARGS...)) {
    return WeakCallback<CLASS, ARGS...>(object, method);
}

template<typename CLASS, typename... ARGS>
WeakCallback<CLASS, ARGS...> MakeWeakCallback(const std::shared_ptr<CLASS>& object,
                                               void (CLASS::*method)(ARGS...) const) {
    return WeakCallback<CLASS, ARGS...>(object, method);
}

}