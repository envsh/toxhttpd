#ifndef MDPREVIEW_H
#define MDPREVIEW_H

#include "compat34.h"

#ifdef QT3_BUILD
#include <qtextbrowser.h>
#include <qurl.h>
#else
#include <QTextBrowser>
#include <QUrl>
#endif

class MdPreview : public QTextBrowser {
    Q_OBJECT
public:
    MdPreview(QWidget* parent = 0);
    void setMarkdown(const QString& markdown);
    void clearPreview();

protected:
    void keyPressEvent(QKeyEvent* e);
};

#endif
