#include "photoviewer.h"
#include "compat34.h"
#include <qlayout.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qpainter.h>
#include <qcursor.h>
#include <qapplication.h>
#include <algorithm>
#ifdef QT3_BUILD
#include <qclipboard.h>
#include <qfiledialog.h>
#include <qdesktopwidget.h>
#else
#include <QClipboard>
#include <QFileDialog>
#endif
using std::min;
using std::max;

// ============================================================
// PhotoCanvas
// ============================================================

PhotoCanvas::PhotoCanvas(QWidget* parent, const QPixmap& pixmap)
    : QWidget(parent
#ifdef QT3_BUILD
      , nullptr, WNoAutoErase
#endif
      )
    , m_pixmap(pixmap)
    , m_scale(1.0)
    , m_rotation(0.0)
    , m_offX(0.0)
    , m_offY(0.0)
    , m_fitMode(true)
    , m_showHelp(false)
    , m_dragging(false)
{
#ifndef QT3_BUILD
    setAttribute(Qt::WA_OpaquePaintEvent, true);
#endif
    fitToWindow();
    rebuildCache();
}

void PhotoCanvas::fitToWindow() {
    m_fitMode = true;
    if (m_pixmap.isNull()) { return; }
    double w = (m_rotation == 90.0 || m_rotation == 270.0)
               ? (double)m_pixmap.height() : (double)m_pixmap.width();
    double h = (m_rotation == 90.0 || m_rotation == 270.0)
               ? (double)m_pixmap.width() : (double)m_pixmap.height();
    if (w <= 0.0 || h <= 0.0) { return; }
    double sx = (double)width() / w;
    double sy = (double)height() / h;
    m_scale = min(sx, sy);
    centerImage();
    rebuildCache();
    update();
}

void PhotoCanvas::actualSize() {
    m_fitMode = false;
    m_scale = 1.0;
    centerImage();
    rebuildCache();
    update();
}

void PhotoCanvas::toggleFitMode() {
    if (m_fitMode) {
        actualSize();
    } else {
        fitToWindow();
    }
}

void PhotoCanvas::zoomIn() {
    m_fitMode = false;
    m_scale = m_scale * 1.25;
    if (m_scale < 0.05) { m_scale = 0.05; }
    rebuildCache();
    update();
}

void PhotoCanvas::zoomOut() {
    m_fitMode = false;
    m_scale = m_scale / 1.25;
    if (m_scale < 0.05) { m_scale = 0.05; }
    rebuildCache();
    update();
}

void PhotoCanvas::rotateCW() {
    m_rotation += 90.0;
    if (m_rotation >= 360.0) { m_rotation -= 360.0; }
    if (m_fitMode) {
        fitToWindow();
    } else {
        centerImage();
        rebuildCache();
        update();
    }
}

void PhotoCanvas::rotateCCW() {
    m_rotation -= 90.0;
    if (m_rotation < 0.0) { m_rotation += 360.0; }
    if (m_fitMode) {
        fitToWindow();
    } else {
        centerImage();
        rebuildCache();
        update();
    }
}

void PhotoCanvas::setShowHelp(bool show) {
    m_showHelp = show;
}

void PhotoCanvas::panBy(int dx, int dy) {
    m_offX += dx;
    m_offY += dy;
    update();
}

int PhotoCanvas::zoomPercent() const {
    return qRound(m_scale * 100.0);
}

void PhotoCanvas::centerImage() {
    double iw = (m_rotation == 90.0 || m_rotation == 270.0)
                ? (double)m_pixmap.height() : (double)m_pixmap.width();
    double ih = (m_rotation == 90.0 || m_rotation == 270.0)
                ? (double)m_pixmap.width() : (double)m_pixmap.height();
    m_offX = ((double)width() - iw * m_scale) / 2.0;
    m_offY = ((double)height() - ih * m_scale) / 2.0;
}

void PhotoCanvas::updateCursor() {
    if (m_dragging) {
        setCursor(QCursor(Qt::SizeAllCursor));
    } else {
        setCursor(QCursor(Qt::ArrowCursor));
    }
}

void PhotoCanvas::rebuildCache() {
    if (m_pixmap.isNull()) { m_cachedPixmap = QPixmap(); return; }
    double dw = (m_rotation == 90.0 || m_rotation == 270.0)
                ? (double)m_pixmap.height() : (double)m_pixmap.width();
    double dh = (m_rotation == 90.0 || m_rotation == 270.0)
                ? (double)m_pixmap.width() : (double)m_pixmap.height();
    m_cachedPixmap = QPixmap(qRound(dw * m_scale), qRound(dh * m_scale));
    QPainter cp(&m_cachedPixmap);
    cp.translate(dw * m_scale / 2.0, dh * m_scale / 2.0);
    cp.rotate(m_rotation);
    cp.translate(-m_pixmap.width() * m_scale / 2.0, -m_pixmap.height() * m_scale / 2.0);
    cp.scale(m_scale, m_scale);
    cp.drawPixmap(0, 0, m_pixmap);
}

void PhotoCanvas::paintEvent(QPaintEvent*) {
    {
        QPainter bp(&m_doubleBuffer);
#ifdef QT3_BUILD
        bp.fillRect(rect(), paletteBackgroundColor());
#else
        bp.fillRect(rect(), palette().window().color());
#endif
        if (!m_cachedPixmap.isNull()) {
            bp.drawPixmap(qRound(m_offX), qRound(m_offY), m_cachedPixmap);
        }
        if (m_showHelp) {
#ifdef QT3_BUILD
            bp.fillRect(rect(), QColor(0, 0, 0));
#else
            bp.fillRect(rect(), QColor(0, 0, 0, 180));
#endif
            bp.setPen(Qt::white);
            QFont f = font();
            f.setPointSize(13);
            bp.setFont(f);
            QFontMetrics fm(f);

            static const char* helpLines[] = {
                "快捷键帮助",
                "",
                "q / Esc    关闭",
                "f / F11    全屏切换",
                "s          保存",
                "c / Ctrl+C 复制到剪贴板",
                ">          顺时针旋转 90度",
                "<          逆时针旋转 90度",
                "上 / +     放大",
                "下 / -     缩小",
                "*          实际大小 100%",
                "/          适应窗口",
                "Z          切换自动适应",
                "h          显示/隐藏帮助",
            };
            int numLines = sizeof(helpLines) / sizeof(helpLines[0]);
            int lineH = fm.lineSpacing() + 4;
            int textH = numLines * lineH;
            int y0 = (height() - textH) / 2 + fm.ascent();
            for (int i = 0; i < numLines; i++) {
                QString s = qFromUtf8(helpLines[i]);
                int tw = fm.width(s);
                bp.drawText((width() - tw) / 2, y0 + i * lineH, s);
            }
        }
    }
    QPainter p(this);
    p.drawPixmap(0, 0, m_doubleBuffer);
}

void PhotoCanvas::wheelEvent(QWheelEvent* event) {
    if (m_pixmap.isNull()) { return; }
    int delta = event->delta();
    if (delta == 0) { return; }

    double oldScale = m_scale;
    double factor = (delta > 0) ? 1.25 : 0.8;
    double newScale = max(0.05, m_scale * factor);

    double cx = (double)event->x() - m_offX;
    double cy = (double)event->y() - m_offY;
    m_offX = (double)event->x() - cx * (newScale / oldScale);
    m_offY = (double)event->y() - cy * (newScale / oldScale);

    m_scale = newScale;
    m_fitMode = false;
    rebuildCache();
    update();
}

void PhotoCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStart = event->pos();
        m_dragOffX = m_offX;
        m_dragOffY = m_offY;
        updateCursor();
    }
}

void PhotoCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging) {
        m_offX = m_dragOffX + (double)(event->x() - m_dragStart.x());
        m_offY = m_dragOffY + (double)(event->y() - m_dragStart.y());
        update();
    }
}

void PhotoCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        updateCursor();
    }
}

void PhotoCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        toggleFitMode();
    }
}

void PhotoCanvas::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_doubleBuffer = QPixmap(size());
    if (m_fitMode) {
        fitToWindow();
    } else {
        rebuildCache();
        update();
    }
}

// ============================================================
// PhotoViewer
// ============================================================

PhotoViewer::PhotoViewer(QWidget* parent, const QPixmap& pixmap)
    : QDialog(parent
#ifdef QT3_BUILD
      , nullptr, false, WDestructiveClose
#endif
      )
    , m_canvas(0)
    , m_toolbar(0)
    , m_statusBar(0)
    , m_statusLabel(0)
    , m_fullscreen(false)
    , m_savedX(0)
    , m_savedY(0)
    , m_savedW(800)
    , m_savedH(600)
    , m_origPixmap(pixmap)
{
#ifndef QT3_BUILD
    setAttribute(Qt::WA_DeleteOnClose);
#endif
    m_canvas = new PhotoCanvas(this, pixmap);

    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->setMargin(0);
    lay->setSpacing(0);

    lay->addWidget(m_canvas, 1);

    setupToolbar(lay);

    m_statusBar = new QWidget(this);
    QHBoxLayout* sbar = new QHBoxLayout(m_statusBar);
    sbar->setMargin(0);
    sbar->setSpacing(0);
    m_statusBar->setFixedHeight(24);
    m_statusLabel = new QLabel(m_statusBar);
    sbar->addWidget(m_statusLabel);
    sbar->addStretch();
    lay->addWidget(m_statusBar);

    resize(800, 600);
    updateTitle();
    updateStatus();
    m_canvas->setFocus();
}

void PhotoViewer::setupToolbar(QVBoxLayout* lay) {
    m_toolbar = new QWidget(this);
    QHBoxLayout* h = new QHBoxLayout(m_toolbar);
    h->setMargin(0);
    h->setSpacing(2);

    struct BtnDef {
        const char* text;
        const char* slot;
    };
    BtnDef btns[] = {
        { "放大 ↑", SLOT(onZoomIn()) },
        { "缩小 ↓", SLOT(onZoomOut()) },
        { "适应 /", SLOT(onFitWindow()) },
        { "实际 *", SLOT(onActualSize()) },
        { "↻ >",   SLOT(onRotateCW()) },
        { "↺ <",   SLOT(onRotateCCW()) },
        { "复制 c", SLOT(onCopy()) },
        { "保存 s", SLOT(onSave()) },
        { "全屏 f", SLOT(onFullscreen()) },
        { "帮助 h", SLOT(onToggleHelp()) },
    };
    int numBtns = sizeof(btns) / sizeof(btns[0]);
    for (int i = 0; i < numBtns; i++) {
        QPushButton* btn = new QPushButton(qFromUtf8(btns[i].text), m_toolbar);
        connect(btn, SIGNAL(clicked()), this, btns[i].slot);
#ifdef QT3_BUILD
        btn->setFocusPolicy(QWidget::NoFocus);
#else
        btn->setFocusPolicy(Qt::NoFocus);
#endif
        h->addWidget(btn);
    }
    h->addStretch();
    lay->addWidget(m_toolbar);
}

void PhotoViewer::updateTitle() {
    QString title = qFromUtf8("PhotoViewer  —  ")
        + QString::number(m_origPixmap.width()) + " × "
        + QString::number(m_origPixmap.height())
        + qFromUtf8("  —  缩放: ") + QString::number(m_canvas->zoomPercent()) + "%";
    if (m_fullscreen) {
        title += qFromUtf8("  [全屏]");
    }
    qSetWindowTitle(this, title);
}

void PhotoViewer::updateStatus() {
    if (!m_statusLabel) { return; }
    QString s = QString::number(m_origPixmap.width())
        + " × " + QString::number(m_origPixmap.height())
        + qFromUtf8("  |  缩放: ") + QString::number(m_canvas->zoomPercent()) + "%";
    if (m_canvas->showHelp()) {
        s += qFromUtf8("  |  [?]");
    }
    m_statusLabel->setText(s);
}

void PhotoViewer::keyPressEvent(QKeyEvent* e) {
#ifdef QT3_BUILD
    uint mod = e->state();
    uint shift = Qt::ShiftButton;
#else
    Qt::KeyboardModifiers mod = e->modifiers();
    Qt::KeyboardModifiers shift = Qt::ShiftModifier;
#endif

    bool shiftPressed = (mod & shift) ? true : false;

    switch (e->key()) {
    case Qt::Key_Q:
    case Qt::Key_Escape:
        close();
        return;
    case Qt::Key_F:
    case Qt::Key_F11:
        onFullscreen();
        return;
    case Qt::Key_S:
        onSave();
        return;
    case Qt::Key_C:
        onCopy();
        return;
    case Qt::Key_Up:
    case Qt::Key_Equal:
        onZoomIn();
        return;
    case Qt::Key_Down:
    case Qt::Key_Minus:
        onZoomOut();
        return;
    case Qt::Key_Left:
        m_canvas->panBy(-20, 0);
        return;
    case Qt::Key_Right:
        m_canvas->panBy(20, 0);
        return;
    case Qt::Key_8:
        if (shiftPressed) {
            onActualSize();
            return;
        }
        break;
    case Qt::Key_Slash:
        onFitWindow();
        return;
    case Qt::Key_Period:
        if (shiftPressed) {
            onRotateCW();
            return;
        }
        break;
    case Qt::Key_Comma:
        if (shiftPressed) {
            onRotateCCW();
            return;
        }
        break;
    case Qt::Key_Z:
        if (shiftPressed) {
            m_canvas->toggleFitMode();
            updateTitle();
            updateStatus();
            return;
        }
        break;
    case Qt::Key_H:
        onToggleHelp();
        return;
    default:
        break;
    }
    QDialog::keyPressEvent(e);
}

PhotoViewer::~PhotoViewer() {
    qDebug("PhotoViewer destroyed, origPixmap=%dx%d",
           m_origPixmap.width(), m_origPixmap.height());
}

void PhotoViewer::closeEvent(QCloseEvent* event) {
    QDialog::closeEvent(event);
}

void PhotoViewer::onSave() {
#ifdef QT3_BUILD
    QString path = QFileDialog::getSaveFileName(
        qGetHomePath(), qFromUtf8("Images (*.png *.jpg)"), this);
#else
    QString path = QFileDialog::getSaveFileName(
        this, qFromUtf8("保存图片"), qGetHomePath(),
        qFromUtf8("Images (*.png *.jpg)"));
#endif
    if (!path.isEmpty()) {
#ifdef QT3_BUILD
        m_origPixmap.save(path, "PNG");
#else
        m_origPixmap.save(path);
#endif
    }
}

void PhotoViewer::onCopy() {
    QApplication::clipboard()->setPixmap(m_origPixmap);
}

void PhotoViewer::onZoomIn() {
    m_canvas->zoomIn();
    updateTitle();
    updateStatus();
}

void PhotoViewer::onZoomOut() {
    m_canvas->zoomOut();
    updateTitle();
    updateStatus();
}

void PhotoViewer::onFitWindow() {
    m_canvas->fitToWindow();
    updateTitle();
    updateStatus();
}

void PhotoViewer::onActualSize() {
    m_canvas->actualSize();
    updateTitle();
    updateStatus();
}

void PhotoViewer::onRotateCW() {
    m_canvas->rotateCW();
    updateTitle();
    updateStatus();
}

void PhotoViewer::onRotateCCW() {
    m_canvas->rotateCCW();
    updateTitle();
    updateStatus();
}

void PhotoViewer::onFullscreen() {
    if (!m_fullscreen) {
        m_savedX = x();
        m_savedY = y();
        m_savedW = width();
        m_savedH = height();
        m_toolbar->hide();
        m_statusBar->hide();
#ifdef QT3_BUILD
        setGeometry(QApplication::desktop()->screenGeometry(
            QApplication::desktop()->screenNumber(this)));
#else
        m_savedFlags = windowFlags();
        m_savedGeo = saveGeometry();
        // QDialog::showFullScreen() ignored by some X11 WMs for Qt::Dialog type.
        // Fix: temporarily change type to Qt::Window, per:
        //   stackoverflow.com/questions/12645880  (2012, 76+ votes)
        //   doc.qt.io/archives/qt-4.8/qwidget.html#saveGeometry
        setWindowFlags(Qt::Window);
        showFullScreen();
#endif
        m_fullscreen = true;
    } else {
        m_toolbar->show();
        m_statusBar->show();
#ifdef QT3_BUILD
        setGeometry(m_savedX, m_savedY, m_savedW, m_savedH);
#else
        // Restore flags + geometry. restoreGeometry() handles frame offset.
        // show() on a hidden window with restored flags re-maps normally.
        setWindowFlags(m_savedFlags);
        restoreGeometry(m_savedGeo);
        show();
#endif
        m_fullscreen = false;
    }
    updateTitle();
}

void PhotoViewer::onToggleHelp() {
    m_canvas->setShowHelp(!m_canvas->showHelp());
    m_canvas->update();
    updateStatus();
}
