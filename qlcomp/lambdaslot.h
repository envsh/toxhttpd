#ifndef LAMBDASLOT_H
#define LAMBDASLOT_H

#include "compat34.h"
#include <functional>

/*
 * LambdaSlot — 零参数 lambda 槽代理
 *
 * 基于 Evan Teran (blog.codef00.com, 2011) 和
 * caetanus/lambda-connect-qt4 (github.com/caetanus/lambda-connect-qt4, 2013)
 * 的模式：QObject 子类 + std::function<void()> + 空壳 slot call()。
 *
 * parent 设为 sender，sender 析构时自动清理辅助对象。
 */
class LambdaSlot : public QObject {
    Q_OBJECT
public:
    LambdaSlot(QObject* parent, std::function<void()> fn)
        : QObject(parent), m_fn(std::move(fn)) {}
public slots:
    void call() { m_fn(); }
private:
    std::function<void()> m_fn;
};

#endif
