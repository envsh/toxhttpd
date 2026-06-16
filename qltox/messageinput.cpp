#include "messageinput.h"
#include "translator.h"
#include <qmessagebox.h>
#ifdef QT3_BUILD
#include <qfile.h>
#else
#include <QFile>
#endif

#ifdef QT3_BUILD
#include <qdragobject.h>
#include <qclipboard.h>
#include <qdatetime.h>
#include <qimage.h>
#include <qapplication.h>
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
#endif

int MessageInput::s_pasteCounter = 0;

MessageInput::MessageInput(QWidget* parent)
    : QTextEdit(parent), m_isPlaceholderActive(false), m_historyIndex(-1) {
#ifdef QT3_BUILD
    setTextFormat(Qt::PlainText);
#else
    setAcceptRichText(false);
#endif
}

void MessageInput::setPlaceholderText(const QString& t) {
    m_placeholder = t;
#ifdef QT3_BUILD
    bool empty = text().isEmpty();
#else
    bool empty = toPlainText().isEmpty();
#endif
    if (m_isPlaceholderActive || empty) {
        m_isPlaceholderActive = true;
#ifdef QT3_BUILD
        QTextEdit::setText(m_placeholder);
#else
        QTextEdit::setPlainText(m_placeholder);
#endif
    }
}

QString MessageInput::placeholderText() const {
    return m_placeholder;
}

void MessageInput::clearPlaceholder() {
    if (m_isPlaceholderActive) {
        m_isPlaceholderActive = false;
#ifdef QT3_BUILD
        QTextEdit::setText(QString());
#else
        QTextEdit::setPlainText(QString());
#endif
    }
}

void MessageInput::focusInEvent(QFocusEvent* e) {
    if (m_isPlaceholderActive) {
        m_isPlaceholderActive = false;
#ifdef QT3_BUILD
        QTextEdit::setText(QString());
#else
        QTextEdit::setPlainText(QString());
#endif
    }
    QTextEdit::focusInEvent(e);
}

void MessageInput::focusOutEvent(QFocusEvent* e) {
#ifdef QT3_BUILD
    QString t = text();
#else
    QString t = toPlainText();
#endif
    if (t.isEmpty() && !m_placeholder.isEmpty()) {
        m_isPlaceholderActive = true;
#ifdef QT3_BUILD
        QTextEdit::setText(m_placeholder);
#else
        QTextEdit::setPlainText(m_placeholder);
#endif
    }
    QTextEdit::focusOutEvent(e);
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
            int ret = QMessageBox::question(this, _("confirm"),
                        _("confirm_send_file").arg(path),
                        QMessageBox::Yes, QMessageBox::No);
            if (ret == QMessageBox::Yes) { emit filePasteRequested(path); return true; }
            return false;
        }
    }
    if (QImageDrag::canDecode(src)) {
        QImage img;
        if (QImageDrag::decode(src, img)) {
            QString tmpPath = QString("/tmp/toxhttpd_paste_%1.png")
                              .arg(s_pasteCounter++);
            img.save(tmpPath, "PNG");
            int ret = QMessageBox::question(this, _("confirm"),
                        _("confirm_send_file").arg(tmpPath),
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
                int ret = QMessageBox::question(this, _("confirm"),
                            _("confirm_send_file").arg(text),
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
                int ret = QMessageBox::question(this, _("confirm"),
                            _("confirm_send_file").arg(path),
                            QMessageBox::Yes, QMessageBox::No);
                if (ret == QMessageBox::Yes) { emit filePasteRequested(path); return true; }
                return false;
            }
        }
    }
    if (data->hasImage()) {
        QImage img = data->imageData().value<QImage>();
        if (!img.isNull()) {
            QString tmpPath = QString("/tmp/toxhttpd_paste_%1.png")
                              .arg(s_pasteCounter++);
            img.save(tmpPath, "PNG");
            int ret = QMessageBox::question(this, _("confirm"),
                        _("confirm_send_file").arg(tmpPath),
                        QMessageBox::Yes, QMessageBox::No);
            if (ret == QMessageBox::Yes) { emit filePasteRequested(tmpPath); return true; }
            return false;
        }
    }
    {
        QString text = data->text().trimmed();
        if (!text.isEmpty() && QFile::exists(text)) {
            int ret = QMessageBox::question(this, _("confirm"),
                        _("confirm_send_file").arg(text),
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
