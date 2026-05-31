#ifndef CUSTOMTITLEBAR_H
#define CUSTOMTITLEBAR_H

#include "compat34.h"
#include <qlabel.h>

class FramelessHelper;

class CustomTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit CustomTitleBar(QWidget* parent = 0);
    void connectFramelessHelper(FramelessHelper* helper);
    void setLabel(const QString& text);
    QString label() const;

private:
    QPushButton* sysBtn;
    QLabel* titleLabel;
    QPushButton* minBtn;
    QPushButton* maxBtn;
    QPushButton* closeBtn;
};

#endif
