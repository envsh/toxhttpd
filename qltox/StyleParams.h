#ifndef STYLEPARAMS_H
#define STYLEPARAMS_H

#ifdef QT3_BUILD
#include <qcolor.h>
#include <qnamespace.h>
#else
#include <QColor>
#include <Qt>
#endif

#include <vector>

enum CompositingMode { AlphaBlend, ColorBlend, JumpCut };

struct StyleParams {
    struct Palette {
        QColor windowBg;
        QColor surfaceBg;
        QColor baseBg;
        QColor hoverBg;
        QColor activeBg;
        QColor textPrimary;
        QColor textMuted;
        QColor textDisabled;
        QColor accent;
        QColor accentText;
        QColor border;
        QColor borderFocus;
        QColor link;
        QColor scrollbarSlider;
        QColor scrollbarHover;
        Palette();
    };
    Palette dark;
    Palette light;
    int buttonRadius;
    int inputRadius;
    int scrollbarWidth;
    int spacing;
    int touchTarget;
    enum ScrollbarMode { AlwaysFaint, OverlayFade } scrollbarMode;
    enum ButtonStyle { Flat, Border, Capsule, Elevated } buttonStyle;
    CompositingMode compositingMode;
    StyleParams();

    struct Definition {
        const char* id;
        const char* displayKey;
        StyleParams (*factory)(bool dark);
    };
    static void registerStyle(const Definition& def);
    static const std::vector<Definition>& registeredStyles();
    static StyleParams make(const char* id, bool dark);
};

extern const StyleParams* g_activeParams;

#endif
