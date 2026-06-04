#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include "StyleParams.h"

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
    static void setDarkMode(bool dark);
    static bool isDarkMode() { return m_darkMode; }
    static void setStyle(const char* id, bool dark);
    static const char* styleId() { return m_styleId; }
    
private:
    static bool m_darkMode;
    static const char* m_styleId;
};

#endif
