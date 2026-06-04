#ifndef TOASTWIDGET_H
#define TOASTWIDGET_H

#include "compat34.h"

class ToastWidget : public QWidget {
public:
    static void show(QWidget* parent, const QString& text, int durationMs = 2000);

protected:
    void paintEvent(QPaintEvent* event);
    void timerEvent(QTimerEvent* event);
    void mousePressEvent(QMouseEvent* event);

private:
    ToastWidget(QWidget* parent, const QString& text, int durationMs);
    void positionAtBottom();

    QString m_text;
    int m_timerId;
};

#endif
