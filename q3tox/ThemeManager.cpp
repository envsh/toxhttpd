#include "ThemeManager.h"

#ifdef QT3_BUILD
#include <qapplication.h>
#include <qpalette.h>
#include <qcolor.h>
#else
#include <QApplication>
#include <QPalette>
#include <QColor>
#endif

bool ThemeManager::m_darkMode = false;

void ThemeManager::setDarkMode(bool dark) {
    m_darkMode = dark;
}

void ThemeManager::applyTheme(bool darkMode) {
    m_darkMode = darkMode;
    
#ifdef QT3_BUILD
    if (darkMode) {
        QColor bg(53, 53, 53);
        QColor fg(212, 212, 212);
        QColor btn(53, 53, 53);
        QColor highlight(42, 130, 218);
        
        QPalette pal;
        pal.setColor(QPalette::Active, QColorGroup::Background, bg);
        pal.setColor(QPalette::Active, QColorGroup::Foreground, fg);
        pal.setColor(QPalette::Active, QColorGroup::Base, QColor(25, 25, 25));
        pal.setColor(QPalette::Active, QColorGroup::Text, fg);
        pal.setColor(QPalette::Active, QColorGroup::Button, btn);
        pal.setColor(QPalette::Active, QColorGroup::ButtonText, fg);
        pal.setColor(QPalette::Active, QColorGroup::Highlight, highlight);
        pal.setColor(QPalette::Active, QColorGroup::HighlightedText, QColor(240, 240, 240));
        pal.setColor(QPalette::Active, QColorGroup::Link, highlight);
        
        pal.setColor(QPalette::Inactive, QColorGroup::Background, bg);
        pal.setColor(QPalette::Inactive, QColorGroup::Foreground, fg);
        pal.setColor(QPalette::Inactive, QColorGroup::Base, QColor(25, 25, 25));
        pal.setColor(QPalette::Inactive, QColorGroup::Text, fg);
        pal.setColor(QPalette::Inactive, QColorGroup::Button, btn);
        pal.setColor(QPalette::Inactive, QColorGroup::ButtonText, fg);
        
        pal.setColor(QPalette::Disabled, QColorGroup::Background, bg);
        pal.setColor(QPalette::Disabled, QColorGroup::Foreground, QColor(127, 127, 127));
        pal.setColor(QPalette::Disabled, QColorGroup::Button, btn);
        pal.setColor(QPalette::Disabled, QColorGroup::Text, QColor(127, 127, 127));
        
        qApp->setPalette(pal);
    } else {
        qApp->setPalette(QPalette());
    }
#else
    if (darkMode) {
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, QColor(212, 212, 212));
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::Text, QColor(212, 212, 212));
        darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ButtonText, QColor(212, 212, 212));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, QColor(240, 240, 240));
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        qApp->setPalette(darkPalette);
    } else {
        qApp->setPalette(QPalette());
    }
#endif
}

void ThemeManager::applyTheme(QWidget* widget, bool darkMode) {
    Q_UNUSED(widget);
    applyTheme(darkMode);
}