#ifndef MYSCROLLAREA_H
#define MYSCROLLAREA_H

#include <QskScrollArea.h>
#include <QPointF>

class MyScrollArea : public QskScrollArea
{
    Q_OBJECT
public:
    explicit MyScrollArea(QQuickItem* parent = nullptr);

    QPointF lastTouchScenePos() const { return m_touchScenePos; }

protected:
    bool childMouseEventFilter(QQuickItem* child, QEvent* event) override;

private:
    static constexpr qreal DRAG_THRESHOLD = 20.0;

    QPointF m_touchStartScene;
    QPointF m_scrollStartPos;
    QPointF m_touchScenePos;
    bool m_touchActive = false;
    bool m_scrolling = false;
};

#endif
