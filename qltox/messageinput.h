#ifndef MESSAGEINPUT_H
#define MESSAGEINPUT_H

#include "compat34.h"

class MessageInput : public QTextEdit {
    Q_OBJECT
public:
    MessageInput(QWidget* parent = 0);

    void setPlaceholderText(const QString& t);
    QString placeholderText() const;
    void clearPlaceholder();
    void saveToHistory(const QString& text);

signals:
    void sendRequested();
    void filePasteRequested(const QString& filePath);

protected:
    void keyPressEvent(QKeyEvent* e);
    void focusInEvent(QFocusEvent* e);
    void focusOutEvent(QFocusEvent* e);
    void dragEnterEvent(QDragEnterEvent* e);
    void dropEvent(QDropEvent* e);

#ifdef QT3_BUILD
    bool handleMimeSource(QMimeSource* src);
#else
    void insertFromMimeData(const QMimeData* source);
    bool handleMimeData(const QMimeData* data);
#endif

private:
    static int s_pasteCounter;
    QString m_placeholder;
    bool m_isPlaceholderActive;
    QStringList m_sentHistory;
    int m_historyIndex;
    QString m_savedDraft;
};

#endif
