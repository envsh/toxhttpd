#include "messageinput.h"
#include "translator.h"
#include <qmessagebox.h>
#ifdef QT3_BUILD
#include <qfile.h>
#include <qfileinfo.h>
#else
#include <QFile>
#include <QFileInfo>
#endif

#ifdef QT3_BUILD
#include <qdragobject.h>
#include <qclipboard.h>
#include <qdatetime.h>
#include <qimage.h>
#include <qapplication.h>
#include <qpainter.h>
#else
#include <QMimeData>
#include <QClipboard>
#include <QDateTime>
#include <QImage>
#include <QUrl>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QPainter>
#endif

int MessageInput::s_pasteCounter = 0;

// 人类可读的文件大小："845 B" / "123.4 KB" / "5.6 MB" / "1.2 GB"
static QString formatFileSize(uint bytes) {
    if (bytes < 1024) { return QString("%1 B").arg(bytes); }
    double kb = bytes / 1024.0;
    if (kb < 1024.0) { return QString("%1 KB").arg(kb, 0, 'f', 1); }
    double mb = kb / 1024.0;
    if (mb < 1024.0) { return QString("%1 MB").arg(mb, 0, 'f', 1); }
    return QString("%1 GB").arg(mb / 1024.0, 0, 'f', 2);
}

MessageInput::MessageInput(QWidget* parent)
    : QTextEdit(parent), m_historyIndex(-1) {
#ifdef QT3_BUILD
    setTextFormat(Qt::PlainText);
    setUndoDepth(32);
#else
    setAcceptRichText(false);
#endif
}

void MessageInput::setPlaceholderText(const QString& t) {
    m_placeholder = t;
    update();
}

QString MessageInput::placeholderText() const {
    return m_placeholder;
}

void MessageInput::clearPlaceholder() {
}

void MessageInput::focusInEvent(QFocusEvent* e) {
    QTextEdit::focusInEvent(e);
}

void MessageInput::focusOutEvent(QFocusEvent* e) {
    QTextEdit::focusOutEvent(e);
}

void MessageInput::paintEvent(QPaintEvent* e) {
    QTextEdit::paintEvent(e);
#ifdef QT3_BUILD
    if (!m_placeholder.isEmpty() && text().isEmpty()) {
        QPainter p(viewport());
        p.setPen(QColor(160, 160, 160));
        QRect r = viewport()->rect();
        r.setLeft(r.left() + 4);
        r.setTop(r.top() + 4);
        r.setRight(r.right() - 4);
        r.setBottom(r.bottom() - 4);
        p.drawText(r, Qt::AlignLeft | Qt::WordBreak, m_placeholder);
    }
#else
    if (!m_placeholder.isEmpty() && toPlainText().isEmpty()) {
        QPainter p(viewport());
        p.setPen(QColor(160, 160, 160));
        QRect r = viewport()->rect().adjusted(4, 4, -4, -4);
        p.drawText(r, Qt::AlignLeft | Qt::TextWordWrap, m_placeholder);
    }
#endif
}

void MessageInput::keyPressEvent(QKeyEvent* e) {
#ifdef QT3_BUILD
    uint mod = e->state();
    uint ctrl = Qt::ControlButton;
    uint shift = Qt::ShiftButton;
    uint alt = Qt::AltButton;
#else
    Qt::KeyboardModifiers mod = e->modifiers();
    Qt::KeyboardModifiers ctrl = Qt::ControlModifier;
    Qt::KeyboardModifiers shift = Qt::ShiftModifier;
    Qt::KeyboardModifiers alt = Qt::AltModifier;
#endif

    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !(mod & shift)) {
        emit sendRequested();
        return;
    }

    // Alt+↑ / Alt+↓ 浏览发送历史
    if (e->key() == Qt::Key_Up && (mod & alt)) {
        if (m_sentHistory.isEmpty()) return;
        if (m_historyIndex == -1) {
#ifdef QT3_BUILD
            m_savedDraft = text();
#else
            m_savedDraft = toPlainText();
#endif
        }
        if (m_historyIndex < m_sentHistory.size() - 1) {
            m_historyIndex++;
#ifdef QT3_BUILD
            QTextEdit::setText(m_sentHistory[m_historyIndex]);
#else
            QTextEdit::setPlainText(m_sentHistory[m_historyIndex]);
#endif
        }
        return;
    }
    if (e->key() == Qt::Key_Down && (mod & alt)) {
        if (m_sentHistory.isEmpty()) return;
        if (m_historyIndex > 0) {
            m_historyIndex--;
#ifdef QT3_BUILD
            QTextEdit::setText(m_sentHistory[m_historyIndex]);
#else
            QTextEdit::setPlainText(m_sentHistory[m_historyIndex]);
#endif
        } else {
            m_historyIndex = -1;
#ifdef QT3_BUILD
            QTextEdit::setText(m_savedDraft);
#else
            QTextEdit::setPlainText(m_savedDraft);
#endif
        }
        return;
    }

    if (e->key() == Qt::Key_A && (mod & ctrl) && !(mod & shift)) {
        selectAll();
        return;
    }

#ifdef QT3_BUILD
    if (e->key() == Qt::Key_V && (mod & ctrl)) {
        QMimeSource* src = QApplication::clipboard()->data();
        if (src && handleMimeSource(src)) return;
    }
#endif

    QTextEdit::keyPressEvent(e);
}

void MessageInput::dragEnterEvent(QDragEnterEvent* e) {
    (void)e;
    e->accept();
}

void MessageInput::dropEvent(QDropEvent* e) {
#ifdef QT3_BUILD
    if (handleMimeSource(e)) {
        e->accept();
        return;
    }
#else
    if (handleMimeData(e->mimeData())) {
        e->acceptProposedAction();
        return;
    }
#endif
    QTextEdit::dropEvent(e);
}

#ifdef QT3_BUILD

bool MessageInput::handleMimeSource(QMimeSource* src) {
    const char* fmt;
    for (int i = 0; (fmt = src->format(i)) != 0; i++) {
        if (qstrcmp(fmt, "text/uri-list") == 0) {
            QByteArray ba = src->encodedData("text/uri-list");
            QString uris = qFromUtf8(ba.data());
            uris = uris.stripWhiteSpace();
            int idx = uris.find('\n');
            if (idx >= 0) { uris = uris.left(idx); }
            QString path = uris;
            if (path.startsWith("file://")) path = path.mid(7);
            if (path.isEmpty() || !QFile::exists(path)) { return false; }
            QString sizeStr = formatFileSize((uint)QFileInfo(path).size());
            int ret = QMessageBox::question(this, _("confirm"),
                        _A("confirm_send_file", QStringList() << path << sizeStr),
                        QMessageBox::Yes, QMessageBox::No);
            if (ret == QMessageBox::Yes) { emit filePasteRequested(path); return true; }
            return false;
        }
    }
    if (QImageDrag::canDecode(src)) {
        QImage img;
        if (QImageDrag::decode(src, img)) {
            QString tmpPath = QString("/tmp/fedox_httpd_paste_%1.png")
                              .arg(s_pasteCounter++);
            img.save(tmpPath, "PNG");
            QString sizeStr = formatFileSize((uint)QFileInfo(tmpPath).size());
            int ret = QMessageBox::question(this, _("confirm"),
                        _A("confirm_send_file", QStringList() << tmpPath << sizeStr),
                        QMessageBox::Yes, QMessageBox::No);
            if (ret == QMessageBox::Yes) { emit filePasteRequested(tmpPath); return true; }
            return false;
        }
    }
    if (QTextDrag::canDecode(src)) {
        QString text;
        if (QTextDrag::decode(src, text)) {
            text = text.stripWhiteSpace();
            if (QFile::exists(text)) {
                QString sizeStr = formatFileSize((uint)QFileInfo(text).size());
                int ret = QMessageBox::question(this, _("confirm"),
                            _A("confirm_send_file", QStringList() << text << sizeStr),
                            QMessageBox::Yes, QMessageBox::No);
                if (ret == QMessageBox::Yes) { emit filePasteRequested(text); return true; }
                return false;
            }
        }
    }
    return false;
}

#else

void MessageInput::insertFromMimeData(const QMimeData* source) {
    if (handleMimeData(source)) return;
    QTextEdit::insertFromMimeData(source);
}

bool MessageInput::handleMimeData(const QMimeData* data) {
    if (data->hasUrls()) {
        QList<QUrl> urls = data->urls();
        for (int i = 0; i < urls.size(); i++) {
            QString path = urls[i].toLocalFile();
            if (!path.isEmpty() && QFile::exists(path)) {
                QString sizeStr = formatFileSize((uint)QFileInfo(path).size());
                int ret = QMessageBox::question(this, _("confirm"),
                            _A("confirm_send_file", QStringList() << path << sizeStr),
                            QMessageBox::Yes, QMessageBox::No);
                if (ret == QMessageBox::Yes) { emit filePasteRequested(path); return true; }
                return false;
            }
        }
    }
    if (data->hasImage()) {
        QImage img = data->imageData().value<QImage>();
        if (!img.isNull()) {
            QString tmpPath = QString("/tmp/fedox_httpd_paste_%1.png")
                              .arg(s_pasteCounter++);
            img.save(tmpPath, "PNG");
            QString sizeStr = formatFileSize((uint)QFileInfo(tmpPath).size());
            int ret = QMessageBox::question(this, _("confirm"),
                        _A("confirm_send_file", QStringList() << tmpPath << sizeStr),
                        QMessageBox::Yes, QMessageBox::No);
            if (ret == QMessageBox::Yes) { emit filePasteRequested(tmpPath); return true; }
            return false;
        }
    }
    {
        QString text = data->text().trimmed();
        if (!text.isEmpty() && QFile::exists(text)) {
            QString sizeStr = formatFileSize((uint)QFileInfo(text).size());
            int ret = QMessageBox::question(this, _("confirm"),
                        _A("confirm_send_file", QStringList() << text << sizeStr),
                        QMessageBox::Yes, QMessageBox::No);
            if (ret == QMessageBox::Yes) { emit filePasteRequested(text); return true; }
            return false;
        }
    }
    return false;
}

#endif

void MessageInput::saveToHistory(const QString& text) {
    if (text.isEmpty()) return;
    if (!m_sentHistory.isEmpty() && m_sentHistory.first() == text) return;
    m_sentHistory.prepend(text);
    while (m_sentHistory.size() > 10)
        m_sentHistory.pop_back();
    m_historyIndex = -1;
    m_savedDraft = QString();
}
