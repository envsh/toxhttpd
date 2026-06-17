#ifndef FLOATINGPILL_H
#define FLOATINGPILL_H

#include "compat34.h"
#include <qrect.h>
#include <qpainter.h>
#include <functional>

class FloatingPill {
public:
    FloatingPill();
    void setCount(int n);
    int count() const { return m_count; }
    void setHovered(bool h) { m_hovered = h; }
    bool isHovered() const { return m_hovered; }
    QRect rect() const { return m_rect; }
    void setCallback(std::function<void()> cb) { m_onClicked = cb; }
    void paint(QPainter& p, const QRect& parentRect, const QColor& bgColor, const QColor& textColor);
    bool handleClick(const QPoint& parentPos);
private:
    int m_count;
    QRect m_rect;
    bool m_hovered;
    std::function<void()> m_onClicked;
};

#endif
