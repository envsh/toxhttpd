#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QApplication>
#include <QPalette>
#include <QColor>

class ThemeManager {
public:
    static void applyTheme(bool darkMode);
    static void applyTheme(QWidget* widget, bool darkMode);
    static bool isDarkMode();
    
private:
    static bool darkMode;
};

#endif // THEMEMANAGER_H
