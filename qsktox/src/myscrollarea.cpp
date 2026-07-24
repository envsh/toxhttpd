#include "myscrollarea.h"
#include <QskEvent.h>
#include <QEvent>
#include <QTouchEvent>
#include <QtMath>
#include <QGuiApplication>
#include <QStyleHints>

MyScrollArea::MyScrollArea(QQuickItem* parent)
    : QskScrollArea(parent)
{
    auto* hints = QGuiApplication::styleHints();
    m_doubleTapInterval = hints->mouseDoubleClickInterval();
    m_doubleTapDistance = hints->touchDoubleTapDistance();
}

bool MyScrollArea::childMouseEventFilter(QQuickItem* child, QEvent* event)
{
    Q_UNUSED(child)

    switch (event->type()) {
    case QEvent::TouchBegin: {
        auto* te = static_cast<QTouchEvent*>(event);
        if (!te->points().isEmpty()) {
            QPointF scenePos = te->points().first().scenePosition();
            m_touchStartScene = scenePos;
            m_touchScenePos = scenePos;
            m_scrollStartPos = scrollPos();
            m_touchActive = true;
            m_scrolling = false;

            ulong now = te->timestamp();
            qreal dx = scenePos.x() - m_lastTapScene.x();
            qreal dy = scenePos.y() - m_lastTapScene.y();
            qreal distSq = dx * dx + dy * dy;
            if (m_lastTapTimestamp > 0
                && (now - m_lastTapTimestamp) < (ulong)m_doubleTapInterval
                && distSq < (qreal)m_doubleTapDistance * m_doubleTapDistance) {
                m_lastTapTimestamp = 0;
                Q_EMIT doubleTapped(scenePos);
            }
        }
        return false;
    }
    case QEvent::TouchUpdate: {
        if (m_touchActive) {
            auto* te = static_cast<QTouchEvent*>(event);
            if (!te->points().isEmpty()) {
                QPointF current = te->points().first().scenePosition();
                m_touchScenePos = current;
                qreal dist = qAbs(current.y() - m_touchStartScene.y());

                if (!m_scrolling && dist >= DRAG_THRESHOLD) {
                    m_scrolling = true;
                    m_scrollStartPos = scrollPos();
                    m_touchStartScene = current;
                }

                if (m_scrolling) {
                    QPointF delta = m_touchStartScene - current;
                    setScrollPos(m_scrollStartPos + delta);
                }
            }
        }
        return false;
    }
    case QEvent::TouchEnd: {
        auto* te = static_cast<QTouchEvent*>(event);
        if (m_touchActive && !m_scrolling) {
            m_lastTapScene = m_touchStartScene;
            m_lastTapTimestamp = te->timestamp();
        }
        m_touchActive = false;
        m_scrolling = false;
        return false;
    }
    default:
        break;
    }

    return false;
}
