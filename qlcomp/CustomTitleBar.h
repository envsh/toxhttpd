#ifndef CUSTOMTITLEBAR_H
#define CUSTOMTITLEBAR_H

#include "compat34.h"
#include "EmbeddedMenuBar.h"
#include <qlabel.h>

class FramelessHelper;

class CustomTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit CustomTitleBar(QWidget* parent = 0);
    void connectFramelessHelper(FramelessHelper* helper);
    void setLabel(const QString& text);
    QString label() const;
    EmbeddedMenuBar* menuBar() const { return menubar; }

signals:
    void appMenuClicked();

private slots:
    void toggleMenu();

private:
    QPushButton* appMenuBtn;
    EmbeddedMenuBar* menubar;
    QPushButton* sysMenuBtn;
    bool m_menuVisible = true;
    QLabel* titleLabel;
    QPushButton* minBtn;
    QPushButton* maxBtn;
    QPushButton* closeBtn;
};

#endif
