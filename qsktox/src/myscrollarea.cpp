#include "myscrollarea.h"
#include <QskEvent.h>
#include <QEvent>
#include <QTouchEvent>
#include <QtMath>

MyScrollArea::MyScrollArea(QQuickItem* parent)
    : QskScrollArea(parent)
{
}

bool MyScrollArea::childMouseEventFilter(QQuickItem* child, QEvent* event)
{
    Q_UNUSED(child)

    switch (event->type()) {
    case QEvent::TouchBegin: {
        auto* te = static_cast<QTouchEvent*>(event);
        if (!te->points().isEmpty()) {
            m_touchStartScene = te->points().first().scenePosition();
            m_touchScenePos = m_touchStartScene;
            m_scrollStartPos = scrollPos();
            m_touchActive = true;
            m_scrolling = false;
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
        m_touchActive = false;
        m_scrolling = false;
        return false;
    }
    default:
        break;
    }

    return false;
}
