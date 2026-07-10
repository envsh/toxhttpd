#ifndef DESKTOP_LYRICS_H
#define DESKTOP_LYRICS_H

// Reference: shinyawhy/QooMusicPlayer DesktopLyricWidget
// https://github.com/shinyawhy/QooMusicPlayer/blob/master/desktoplyricwidget.cpp

#include "compat34.h"
#include <qstring.h>
#include <qcolor.h>
#include <qfont.h>
#include <qpoint.h>
#include <qnamespace.h>
#include <qpair.h>
#include <vector>

struct LrcLine {
    long long time;
    QString text;
};

class DesktopLyrics : public QWidget {
    Q_OBJECT
    Q_PROPERTY(float progress READ progress WRITE setProgress)
public:
    DesktopLyrics();
    ~DesktopLyrics();

    void showLyrics();
    void hideLyrics();
    void setLocked(bool locked);
    bool isLocked() const { return m_locked; }
    void toggleLocked();

    void setLrcText(const QString& lrcContent);
    void setPosition(long long msec);
    void setPlaying(bool playing);

    void setPlayedColor(const QColor& c) { m_playedColor = c; update(); }
    void setUnplayedColor(const QColor& c) { m_unplayedColor = c; update(); }
    void setStrokeColor(const QColor& c) { m_strokeColor = c; update(); }
    void setFontSize(int pt);
    void setAlignment(int align) { m_align = align; update(); }

    float progress() const { return m_animProgress; }
    void setProgress(float p);

signals:
    void positionClicked(long long msec);

protected:
    void paintEvent(QPaintEvent* e);
    void mousePressEvent(QMouseEvent* e);
    void mouseMoveEvent(QMouseEvent* e);
    void mouseReleaseEvent(QMouseEvent* e);
#ifdef QT3_BUILD
    void timerEvent(QTimerEvent* e);
#endif

private:
    void setupWindow();
    void parseLrc(const QString& content, std::vector<LrcLine>& out);
    void drawLine(QPainter& p, const QString& text, int x, int y, float progress);
    void drawTextWithStroke(QPainter& p, const QString& text, int x, int y,
                            const QColor& fillColor);

    std::vector<LrcLine> m_lines;
    long long m_position;
    int m_currentLine;

    float m_animProgress;
    float m_animTarget;
#ifdef QT3_BUILD
    int m_animTimerId;
    class QPixmap* m_backing;
#else
    class QPropertyAnimation* m_anim;
#endif

    QFont m_font;
    QColor m_playedColor;
    QColor m_unplayedColor;
    QColor m_strokeColor;
    int m_align;

    bool m_locked;
    bool m_dragging;
    QPoint m_dragStart;
};

#endif
