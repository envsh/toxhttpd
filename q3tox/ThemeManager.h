#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#ifdef QT3_BUILD
#define THEME_API

#include <qwidget.h>
#include <qapplication.h>
#include <qpalette.h>

#else
#define THEME_API Q_DECL_EXPORT

#include <QWidget>
#include <QApplication>
#include <QPalette>
#endif

class THEME_API ThemeManager {
public:
    static void applyTheme(bool darkMode);
    static void applyTheme(QWidget* widget, bool darkMode);
    static void setDarkMode(bool dark);
    static bool isDarkMode() { return m_darkMode; }
    
private:
    static bool m_darkMode;
};

#endif
