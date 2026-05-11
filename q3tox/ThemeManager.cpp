#include "ThemeManager.h"
#include "LimeStyle.h"

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
const char* ThemeManager::m_styleId = "qtFusion";

void ThemeManager::setDarkMode(bool dark) {
    m_darkMode = dark;
}

void ThemeManager::setStyle(const char* id, bool dark) {
    static StyleParams params;
    params = StyleParams::make(id, dark);
    g_activeParams = &params;
    m_styleId = id;
    m_darkMode = dark;

    const auto& pal = dark ? params.dark : params.light;

#ifdef QT3_BUILD
    QPalette qpal;
    qpal.setColor(QPalette::Active, QColorGroup::Background, pal.windowBg);
    qpal.setColor(QPalette::Active, QColorGroup::Foreground, pal.textPrimary);
    qpal.setColor(QPalette::Active, QColorGroup::Base, pal.baseBg);
    qpal.setColor(QPalette::Active, QColorGroup::Text, pal.textPrimary);
    qpal.setColor(QPalette::Active, QColorGroup::Button, pal.surfaceBg);
    qpal.setColor(QPalette::Active, QColorGroup::ButtonText, pal.textPrimary);
    qpal.setColor(QPalette::Active, QColorGroup::Highlight, pal.accent);
    qpal.setColor(QPalette::Active, QColorGroup::HighlightedText, pal.accentText);
    qpal.setColor(QPalette::Active, QColorGroup::Link, pal.link);
    qpal.setColor(QPalette::Active, QColorGroup::Light, pal.hoverBg);
    qpal.setColor(QPalette::Active, QColorGroup::Midlight, lerpColor(pal.surfaceBg, pal.hoverBg, 0.5f));
    qpal.setColor(QPalette::Active, QColorGroup::Mid, pal.surfaceBg);
    qpal.setColor(QPalette::Active, QColorGroup::Dark, lerpColor(pal.windowBg, pal.surfaceBg, 0.3f));
    qpal.setColor(QPalette::Active, QColorGroup::Shadow, pal.windowBg);
    qpal.setColor(QPalette::Active, QColorGroup::BrightText, QColor("#ffffff"));

    qpal.setColor(QPalette::Inactive, QColorGroup::Background, pal.windowBg);
    qpal.setColor(QPalette::Inactive, QColorGroup::Foreground, pal.textPrimary);
    qpal.setColor(QPalette::Inactive, QColorGroup::Base, pal.baseBg);
    qpal.setColor(QPalette::Inactive, QColorGroup::Text, pal.textPrimary);
    qpal.setColor(QPalette::Inactive, QColorGroup::Button, pal.surfaceBg);
    qpal.setColor(QPalette::Inactive, QColorGroup::ButtonText, pal.textPrimary);

    qpal.setColor(QPalette::Disabled, QColorGroup::Background, pal.windowBg);
    qpal.setColor(QPalette::Disabled, QColorGroup::Foreground, pal.textDisabled);
    qpal.setColor(QPalette::Disabled, QColorGroup::Text, pal.textDisabled);
    qpal.setColor(QPalette::Disabled, QColorGroup::Button, pal.surfaceBg);
    qpal.setColor(QPalette::Disabled, QColorGroup::ButtonText, pal.textDisabled);

    qApp->setPalette(qpal);
#else
    QPalette qpal;
    qpal.setColor(QPalette::Window, pal.windowBg);
    qpal.setColor(QPalette::WindowText, pal.textPrimary);
    qpal.setColor(QPalette::Base, pal.baseBg);
    qpal.setColor(QPalette::Text, pal.textPrimary);
    qpal.setColor(QPalette::Button, pal.surfaceBg);
    qpal.setColor(QPalette::ButtonText, pal.textPrimary);
    qpal.setColor(QPalette::Highlight, pal.accent);
    qpal.setColor(QPalette::HighlightedText, pal.accentText);
    qpal.setColor(QPalette::Link, pal.link);
    qpal.setColor(QPalette::Light, pal.hoverBg);
    qpal.setColor(QPalette::Midlight, lerpColor(pal.surfaceBg, pal.hoverBg, 0.5f));
    qpal.setColor(QPalette::Mid, pal.surfaceBg);
    qpal.setColor(QPalette::Dark, lerpColor(pal.windowBg, pal.surfaceBg, 0.3f));
    qpal.setColor(QPalette::Shadow, pal.windowBg);
    qpal.setColor(QPalette::BrightText, QColor("#ffffff"));

    qApp->setPalette(qpal);
#endif
}

void ThemeManager::applyTheme(bool darkMode) {
    setStyle(m_styleId, darkMode);
}

void ThemeManager::applyTheme(QWidget* widget, bool darkMode) {
    Q_UNUSED(widget);
    applyTheme(darkMode);
}