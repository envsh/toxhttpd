#include "desktoplyrics.h"
#ifdef QT3_BUILD
#include <qpainter.h>
#include <qapplication.h>
#else
#include <QPainter>
#include <QApplication>
#include <QDesktopWidget>
#include <QPropertyAnimation>
#include <QEasingCurve>
#endif

DesktopLyrics::DesktopLyrics()
    : QWidget(0
#ifdef QT3_BUILD
      , 0, WStyle_StaysOnTop | WStyle_Customize | WStyle_NoBorder | WStyle_Tool
#else
      , Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool
#endif
      )
    , m_position(0)
    , m_currentLine(-1)
    , m_animProgress(0.0f)
    , m_animTarget(0.0f)
#ifdef QT3_BUILD
    , m_animTimerId(0)
    , m_backing(0)
#else
    , m_anim(0)
#endif
    , m_playedColor(0x00, 0xB4, 0xD8)
    , m_unplayedColor(0xBB, 0xBB, 0xBB)
    , m_strokeColor(0x00, 0x00, 0x00)
    , m_align(Qt::AlignCenter)
    , m_locked(false)
    , m_dragging(false)
{
    setFixedSize(600, 80);
    m_font.setPixelSize(28);
    m_font.setBold(true);
#ifdef QT3_BUILD
    m_backing = new QPixmap(600, 80);
#endif
    setupWindow();
}

DesktopLyrics::~DesktopLyrics()
{
#ifdef QT3_BUILD
    if (m_animTimerId) {
        killTimer(m_animTimerId);
    }
    delete m_backing;
#else
    delete m_anim;
#endif
}

void DesktopLyrics::setupWindow()
{
#ifdef QT3_BUILD
    setBackgroundMode(Qt::NoBackground);
    setAutoMask(false);
    setEraseColor(QColor(0, 0, 0));
#else
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
#endif
}

void DesktopLyrics::showLyrics()
{
    QWidget::show();
    QRect screen = QApplication::desktop()->screenGeometry();
    move((screen.width() - width()) / 2, 50);
    raise();
}

void DesktopLyrics::hideLyrics()
{
    hide();
}

void DesktopLyrics::setLocked(bool locked)
{
    m_locked = locked;
}

void DesktopLyrics::toggleLocked()
{
    m_locked = !m_locked;
}

void DesktopLyrics::setFontSize(int pt)
{
    m_font.setPointSize(pt);
    update();
}

void DesktopLyrics::setProgress(float p)
{
    m_animProgress = p;
    update();
}

void DesktopLyrics::setPlaying(bool playing)
{
    if (!playing) {
        m_animProgress = 0.0f;
        m_animTarget = 0.0f;
        m_currentLine = -1;
        m_position = 0;
        update();
    }
}

static void lrcTrim(QString& s)
{
    if (s.isEmpty()) return;
    int start = 0;
    while (start < (int)s.length() && s[start] == QChar(' ')) start++;
    int end = (int)s.length() - 1;
    while (end >= 0 && s[end] == QChar(' ')) end--;
    if (start > 0 || end < (int)s.length() - 1) {
        s = s.mid(start, end - start + 1);
    }
}

void DesktopLyrics::parseLrc(const QString& content, std::vector<LrcLine>& out)
{
    out.clear();
    if (content.isEmpty()) return;

    QStringList rawLines = qSplit(content, "\n");
    for (int li = 0; li < rawLines.size(); li++) {
        QString line = rawLines[li];
        lrcTrim(line);
        if (line.isEmpty()) continue;

        std::vector<long long> times;
        int pos = 0;
        int len = (int)line.length();

        while (pos < len && line[pos] == QChar('[')) {
            int close = -1;
            for (int j = pos + 1; j < len; j++) {
                if (line[j] == QChar(']')) {
                    close = j;
                    break;
                }
            }
            if (close < 0) break;

            QString tag = line.mid(pos + 1, close - pos - 1);
            pos = close + 1;

            int colon = -1;
            for (int k = 0; k < (int)tag.length(); k++) {
                if (tag[k] == QChar(':')) {
                    colon = k;
                    break;
                }
            }
            if (colon < 0) continue;

            bool ok1 = false, ok2 = false;
            int min = tag.left(colon).toInt(&ok1);
            int sec = tag.mid(colon + 1).toInt(&ok2);
            if (!ok1 || !ok2) continue;

            int hundredths = 0;
            int dot = -1;
            for (int k = colon + 1; k < (int)tag.length(); k++) {
                if (tag[k] == QChar('.')) {
                    dot = k;
                    break;
                }
            }
            if (dot >= 0) {
                QString frac = tag.mid(dot + 1);
                if (frac.length() >= 2) {
                    hundredths = frac.left(2).toInt();
                } else if (frac.length() == 1) {
                    hundredths = frac.left(1).toInt() * 10;
                }
            }

            long long totalMs = (long long)min * 60000 + (long long)sec * 1000 + (long long)hundredths * 10;
            times.push_back(totalMs);
        }

        if (times.empty()) continue;

        QString text = line.mid(pos);
        lrcTrim(text);
        if (text.isEmpty()) continue;

        for (size_t ti = 0; ti < times.size(); ti++) {
            LrcLine lrc;
            lrc.time = times[ti];
            lrc.text = text;
            out.push_back(lrc);
        }
    }

    for (size_t i = 0; i < out.size(); i++) {
        for (size_t j = i + 1; j < out.size(); j++) {
            if (out[j].time < out[i].time) {
                LrcLine tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }
}

void DesktopLyrics::setLrcText(const QString& lrcContent)
{
    m_lines.clear();
    parseLrc(lrcContent, m_lines);
    m_currentLine = -1;
    m_position = 0;
    m_animProgress = 0.0f;
    m_animTarget = 0.0f;
    update();
}

void DesktopLyrics::setPosition(long long msec)
{
    m_position = msec;
    if (m_lines.empty()) {
        m_currentLine = -1;
        m_animProgress = 0.0f;
        m_animTarget = 0.0f;
        update();
        return;
    }

    int idx = -1;
    for (size_t i = 0; i < m_lines.size(); i++) {
        if (m_lines[i].time <= msec) {
            idx = (int)i;
        } else {
            break;
        }
    }

    if (idx != m_currentLine) {
        m_currentLine = idx;
    }

    if (m_currentLine >= 0 && m_currentLine < (int)m_lines.size() - 1) {
        long long lineStart = m_lines[m_currentLine].time;
        long long lineEnd = m_lines[m_currentLine + 1].time;
        if (lineEnd > lineStart) {
            float target = (float)(msec - lineStart) / (float)(lineEnd - lineStart);
            if (target < 0.0f) target = 0.0f;
            if (target > 1.0f) target = 1.0f;
            m_animTarget = target;
#ifdef QT3_BUILD
            if (!m_animTimerId) {
                m_animTimerId = startTimer(30);
            }
#else
            if (m_anim) {
                m_anim->stop();
                delete m_anim;
                m_anim = 0;
            }
            m_anim = new QPropertyAnimation(this, "progress");
            m_anim->setDuration(250);
            m_anim->setStartValue(m_animProgress);
            m_anim->setEndValue(m_animTarget);
            m_anim->setEasingCurve(QEasingCurve::Linear);
            m_anim->start();
#endif
        } else {
            m_animProgress = 1.0f;
            m_animTarget = 1.0f;
            update();
        }
    } else if (m_currentLine >= 0) {
        m_animProgress = 1.0f;
        m_animTarget = 1.0f;
        update();
    } else {
        m_animProgress = 0.0f;
        m_animTarget = 0.0f;
        update();
    }
}

void DesktopLyrics::drawTextWithStroke(QPainter& p, const QString& text,
                                       int x, int y, const QColor& fillColor)
{
    QPen oldPen = p.pen();
    p.setPen(m_strokeColor);
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            p.drawText(x + dx, y + dy, text);
        }
    }
    p.setPen(fillColor);
    p.drawText(x, y, text);
    p.setPen(oldPen);
}

void DesktopLyrics::drawLine(QPainter& p, const QString& text, int x, int y, float progress)
{
    int textWidth = p.fontMetrics().width(text);

    drawTextWithStroke(p, text, x, y, m_unplayedColor);

    int playedWidth = (int)(textWidth * progress);
    if (playedWidth > 0 && playedWidth < textWidth) {
        p.save();
        p.setClipRect(x, y - 50, playedWidth, 100);
        drawTextWithStroke(p, text, x, y, m_playedColor);
        p.restore();
    } else if (playedWidth >= textWidth) {
        drawTextWithStroke(p, text, x, y, m_playedColor);
    }
}

void DesktopLyrics::paintEvent(QPaintEvent*)
{
#ifdef QT3_BUILD
    if (!m_backing || m_backing->isNull() || m_backing->size() != size()) {
        delete m_backing;
        m_backing = new QPixmap(size());
    }
    QPainter p(m_backing);
#else
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
#endif

#ifdef QT3_BUILD
    QColor bgColor(20, 20, 20);
#else
    QColor bgColor(0, 0, 0, 180);
#endif
    QRect r = rect();
    p.fillRect(r, bgColor);

    if (m_lines.empty() || m_currentLine < 0 || m_currentLine >= (int)m_lines.size()) {
        p.setPen(QColor(0x88, 0x88, 0x88));
        p.setFont(m_font);
        p.drawText(r, Qt::AlignCenter, "No Lyrics");
#ifdef QT3_BUILD
        p.end();
        bitBlt(this, 0, 0, m_backing);
#endif
        return;
    }

    const LrcLine& current = m_lines[m_currentLine];
    QString text = current.text;
    p.setFont(m_font);

    QFontMetrics fm = p.fontMetrics();
    int textWidth = fm.width(text);
    int x = (width() - textWidth) / 2;
    if (x < 4) x = 4;
    int y = height() / 2 + fm.ascent() / 2;

    drawLine(p, text, x, y, m_animProgress);

#ifdef QT3_BUILD
    p.end();
    bitBlt(this, 0, 0, m_backing);
#endif
}

void DesktopLyrics::mousePressEvent(QMouseEvent* e)
{
    if (m_locked) {
        if (!m_lines.empty() && m_currentLine >= 0 && m_currentLine < (int)m_lines.size()) {
            emit positionClicked(m_lines[m_currentLine].time);
        }
        return;
    }
    m_dragging = true;
#ifdef QT3_BUILD
    m_dragStart = QPoint(e->globalX() - geometry().x(),
                         e->globalY() - geometry().y());
#else
    m_dragStart = e->globalPos() - geometry().topLeft();
#endif
}

void DesktopLyrics::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging || m_locked) return;
#ifdef QT3_BUILD
    move(e->globalX() - m_dragStart.x(),
         e->globalY() - m_dragStart.y());
#else
    move(e->globalPos() - m_dragStart);
#endif
}

void DesktopLyrics::mouseReleaseEvent(QMouseEvent*)
{
    m_dragging = false;
}

#ifdef QT3_BUILD
void DesktopLyrics::timerEvent(QTimerEvent*)
{
    float diff = m_animTarget - m_animProgress;
    if (diff < 0.0f) diff = -diff;
    if (diff < 0.005f) {
        m_animProgress = m_animTarget;
        if (m_animTimerId) {
            killTimer(m_animTimerId);
            m_animTimerId = 0;
        }
    } else {
        m_animProgress += (m_animTarget - m_animProgress) * 0.15f;
    }
    update();
}
#endif
