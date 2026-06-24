#ifndef GENERIC_SLOT_H
#define GENERIC_SLOT_H

#include "compat34.h"

// GenericSlot + connect_lambda 使用 Qt4 的 qt_metacall + void**
// Qt3 元对象系统不同（qt_invoke + QUObject），不支持此方案
#ifndef QT3_BUILD

#include <functional>
#include <memory>
#include <tuple>

// ====== C++11 index_sequence ======
template <size_t... Is> struct seq {};
template <size_t N, size_t... Is> struct gen_seq : gen_seq<N-1, N-1, Is...> {};
template <size_t... Is> struct gen_seq<0, Is...> : seq<Is...> {};

// ====== function_traits ======
template <typename F>
struct function_traits : function_traits<decltype(&F::operator())> {};

template <typename R, typename... Args>
struct function_traits<R(*)(Args...)> {
    static constexpr size_t arity = sizeof...(Args);
    template <size_t I>
    struct arg { using type = typename std::tuple_element<I, std::tuple<Args...>>::type; };
};

template <typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...) const> {
    static constexpr size_t arity = sizeof...(Args);
    template <size_t I>
    struct arg { using type = typename std::tuple_element<I, std::tuple<Args...>>::type; };
};

template <typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...)> {
    static constexpr size_t arity = sizeof...(Args);
    template <size_t I>
    struct arg { using type = typename std::tuple_element<I, std::tuple<Args...>>::type; };
};

// ====== expand_args ======
template <typename F, size_t... Is>
void expand_args(F& f, void** args, seq<Is...>) {
    using T = function_traits<typename std::decay<F>::type>;
    f(*static_cast<typename T::template arg<Is>::type*>(args[Is+1])...);
}

// ====== GenericSlot (无 Q_OBJECT, 不被 moc 处理) ======
class GenericSlot : public QObject {
public:
    GenericSlot(QObject* parent, std::function<void(void**)> fn)
        : QObject(parent), m_fn(std::move(fn)) {}

    int qt_metacall(QMetaObject::Call c, int id, void** args);

    static int slotMethodIndex() {
        static int idx = QObject::staticMetaObject.methodCount();
        return idx;
    }

private:
    std::function<void(void**)> m_fn;
};

// ====== connect_lambda — Qt5 风格一行式 API ======
/*
 * 用法：
 *   connect_lambda(btn, SIGNAL(clicked()), []{ qDebug("hi"); });
 *   connect_lambda(slider, SIGNAL(valueChanged(int)), [](int v){ ... });
 *   connect_lambda(dlg, SIGNAL(destroyed(QObject*)), []{ s_ptr = 0; });
 *
 * 注意：只在 Qt4+ 下可用。Qt3 请使用 LambdaSlot 或具名 helper class。
 */
template <typename F>
bool connect_lambda(QObject* sender, const char* signal, F&& func) {
    QByteArray sig = QMetaObject::normalizedSignature(signal + 1);
    int sigIdx = sender->metaObject()->indexOfSignal(sig);
    if (sigIdx < 0)
        return false;

    using F2 = typename std::decay<F>::type;
    auto sp = std::shared_ptr<F2>(new F2(std::forward<F>(func)));
    auto wrapper = [sp](void** args) {
        expand_args(*sp, args, gen_seq<function_traits<F2>::arity>{});
    };

    GenericSlot* gs = new GenericSlot(sender, std::move(wrapper));
    return QMetaObject::connect(sender, sigIdx, gs, GenericSlot::slotMethodIndex());
}

#endif // QT3_BUILD
#endif
