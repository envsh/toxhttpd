#include "messageinput.h"
#include "translator.h"
#include "imageinputconfirm.h"
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

static bool tryLoadImage(const QString& path, QImage& out) {
    out = QImage(path);
    return !out.isNull();
}

enum MimeMode { kModePaste = 0, kModeDrop = 1, kModeShare = 2 };
enum ImageSrc { kSrcPasteImage, kSrcPastePath, kSrcDropImage,
                kSrcDropPath,   kSrcShareImage, kSrcSharePath };

static int kindImage(int mode) {
    if (mode == kModePaste) { return kSrcPasteImage; }
    if (mode == kModeShare) { return kSrcShareImage; }
    return kSrcDropImage;
}

static int kindPath(int mode) {
    if (mode == kModePaste) { return kSrcPastePath; }
    if (mode == kModeShare) { return kSrcSharePath; }
    return kSrcDropPath;
}

static QString srcText(int kind) {
    if (kind == kSrcPasteImage) { return _("paste_image.source.paste_image"); }
    if (kind == kSrcPastePath)  { return _("paste_image.source.paste_path"); }
    if (kind == kSrcDropImage)  { return _("paste_image.source.drop_image"); }
    if (kind == kSrcDropPath)   { return _("paste_image.source.drop_path"); }
    if (kind == kSrcShareImage) { return _("paste_image.source.share_image"); }
    return _("paste_image.source.share_path");
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
        if (src && handleMimeSource(src, kModePaste)) return;
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
    if (handleMimeSource(e, kModeDrop)) {
        e->accept();
        return;
    }
#else
    if (handleMimeData(e->mimeData(), kModeDrop)) {
        e->acceptProposedAction();
        return;
    }
#endif
    QTextEdit::dropEvent(e);
}

#ifdef QT3_BUILD

bool MessageInput::handleMimeSource(QMimeSource* src, int srcMode) {
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
            QImage pv;
            if (tryLoadImage(path, pv)) {
                QString sizeStr = formatFileSize((uint)QFileInfo(path).size());
                ImageInputConfirmDialog dlg(pv, path, sizeStr, srcText(kindPath(srcMode)), this);
                if (dlg.exec() != QDialog::Accepted) { return false; }
                emit filePasteRequested(path, dlg.caption());
                return true;
            }
            QString sizeStr = formatFileSize((uint)QFileInfo(path).size());
            int ret = QMessageBox::question(this, _("confirm"),
                        _A("confirm_send_file", QStringList() << path << sizeStr),
                        QMessageBox::Yes, QMessageBox::No);
            if (ret == QMessageBox::Yes) { emit filePasteRequested(path, QString()); return true; }
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
            ImageInputConfirmDialog dlg(img, tmpPath, sizeStr, srcText(kindImage(srcMode)), this);
            if (dlg.exec() != QDialog::Accepted) {
                QFile::remove(tmpPath);
                return false;
            }
            emit filePasteRequested(tmpPath, dlg.caption());
            return true;
        }
    }
    if (QTextDrag::canDecode(src)) {
        QString text;
        if (QTextDrag::decode(src, text)) {
            text = text.stripWhiteSpace();
            if (QFile::exists(text)) {
                QImage pv;
                if (tryLoadImage(text, pv)) {
                    QString sizeStr = formatFileSize((uint)QFileInfo(text).size());
                    ImageInputConfirmDialog dlg(pv, text, sizeStr, srcText(kindPath(srcMode)), this);
                    if (dlg.exec() != QDialog::Accepted) { return false; }
                    emit filePasteRequested(text, dlg.caption());
                    return true;
                }
                QString sizeStr = formatFileSize((uint)QFileInfo(text).size());
                int ret = QMessageBox::question(this, _("confirm"),
                            _A("confirm_send_file", QStringList() << text << sizeStr),
                            QMessageBox::Yes, QMessageBox::No);
                if (ret == QMessageBox::Yes) { emit filePasteRequested(text, QString()); return true; }
                return false;
            }
        }
    }
    return false;
}

#else

void MessageInput::insertFromMimeData(const QMimeData* source) {
    if (handleMimeData(source, kModePaste)) return;
    QTextEdit::insertFromMimeData(source);
}

bool MessageInput::handleMimeData(const QMimeData* data, int srcMode) {
    if (data->hasUrls()) {
        QList<QUrl> urls = data->urls();
        for (int i = 0; i < urls.size(); i++) {
            QString path = urls[i].toLocalFile();
            if (!path.isEmpty() && QFile::exists(path)) {
                QImage pv;
                if (tryLoadImage(path, pv)) {
                    QString sizeStr = formatFileSize((uint)QFileInfo(path).size());
                    ImageInputConfirmDialog dlg(pv, path, sizeStr, srcText(kindPath(srcMode)), this);
                    if (dlg.exec() != QDialog::Accepted) { return false; }
                    emit filePasteRequested(path, dlg.caption());
                    return true;
                }
                QString sizeStr = formatFileSize((uint)QFileInfo(path).size());
                int ret = QMessageBox::question(this, _("confirm"),
                            _A("confirm_send_file", QStringList() << path << sizeStr),
                            QMessageBox::Yes, QMessageBox::No);
                if (ret == QMessageBox::Yes) { emit filePasteRequested(path, QString()); return true; }
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
            ImageInputConfirmDialog dlg(img, tmpPath, sizeStr, srcText(kindImage(srcMode)), this);
            if (dlg.exec() != QDialog::Accepted) {
                QFile::remove(tmpPath);
                return false;
            }
            emit filePasteRequested(tmpPath, dlg.caption());
            return true;
        }
    }
    {
        QString text = data->text().trimmed();
        if (!text.isEmpty() && QFile::exists(text)) {
            QImage pv;
            if (tryLoadImage(text, pv)) {
                QString sizeStr = formatFileSize((uint)QFileInfo(text).size());
                ImageInputConfirmDialog dlg(pv, text, sizeStr, srcText(kindPath(srcMode)), this);
                if (dlg.exec() != QDialog::Accepted) { return false; }
                emit filePasteRequested(text, dlg.caption());
                return true;
            }
            QString sizeStr = formatFileSize((uint)QFileInfo(text).size());
            int ret = QMessageBox::question(this, _("confirm"),
                        _A("confirm_send_file", QStringList() << text << sizeStr),
                        QMessageBox::Yes, QMessageBox::No);
            if (ret == QMessageBox::Yes) { emit filePasteRequested(text, QString()); return true; }
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
