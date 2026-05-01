#include "ThemeManager.h"

bool ThemeManager::darkMode = true;

void ThemeManager::applyTheme(bool dark) {
    darkMode = dark;
    QApplication* app = qobject_cast<QApplication*>(QApplication::instance());
    if (!app) return;
    
    if (dark) {
        // Dark theme (GitHub style)
        QPalette palette;
        palette.setColor(QPalette::Window, QColor(13, 17, 23)); // #0d1117
        palette.setColor(QPalette::WindowText, QColor(201, 209, 217)); // #c9d1d9
        palette.setColor(QPalette::Base, QColor(22, 27, 34)); // #161b22
        palette.setColor(QPalette::AlternateBase, QColor(33, 38, 45)); // #21262d
        palette.setColor(QPalette::ToolTipBase, QColor(22, 27, 34));
        palette.setColor(QPalette::ToolTipText, QColor(201, 209, 217));
        palette.setColor(QPalette::Text, QColor(201, 209, 217));
        palette.setColor(QPalette::Button, QColor(33, 38, 45));
        palette.setColor(QPalette::ButtonText, QColor(201, 209, 217));
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Highlight, QColor(0, 212, 170)); // #00d4aa
        palette.setColor(QPalette::HighlightedText, Qt::black);
        
        app->setPalette(palette);
        app->setStyleSheet("QToolTip { color: #c9d1d9; background-color: #161b22; "
                          "border: 1px solid #30363d; }");
    } else {
        // Light theme
        QPalette palette;
        palette.setColor(QPalette::Window, Qt::white);
        palette.setColor(QPalette::WindowText, Qt::black);
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
        palette.setColor(QPalette::Text, Qt::black);
        palette.setColor(QPalette::Button, QColor(240, 240, 240));
        palette.setColor(QPalette::ButtonText, Qt::black);
        palette.setColor(QPalette::Highlight, QColor(0, 120, 215));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        
        app->setPalette(palette);
    }
}

void ThemeManager::applyTheme(QWidget* widget, bool dark) {
    if (!widget) return;
    // Apply theme to specific widget if needed
}

bool ThemeManager::isDarkMode() {
    return darkMode;
}
