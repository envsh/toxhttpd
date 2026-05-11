#ifndef STYLEPARAMS_H
#define STYLEPARAMS_H

#ifdef QT3_BUILD
#include <qcolor.h>
#include <qnamespace.h>
#else
#include <QColor>
#include <Qt>
#endif

enum StyleId { StyleQtFusion, StyleMacOS, StyleWindows, StyleMaterial };

enum CompositingMode {
    AlphaBlend,
    ColorBlend,
    JumpCut
};

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

    static StyleParams qtFusion(bool dark);
    static StyleParams macOS(bool dark);
    static StyleParams windows11(bool dark);
    static StyleParams materialYou(bool dark);

    static StyleParams make(StyleId id, bool dark);
};

extern const StyleParams* g_activeParams;

#endif
