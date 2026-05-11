#include "LimeStyle.h"
#include "ThemeManager.h"

const StyleParams* g_activeParams = nullptr;

// ========== Palette defaults ==========

StyleParams::Palette::Palette()
    : windowBg("#161b22"), surfaceBg("#21262d"), baseBg("#161b22"),
      hoverBg("#30363d"), activeBg("#3d444d"), textPrimary("#c9d1d9"),
      textMuted("#8b949e"), textDisabled("#484f58"), accent("#1f6feb"),
      accentText("#ffffff"), border("#30363d"), borderFocus("#1f6feb"),
      scrollbarSlider("#30363d"), scrollbarHover("#3d444d") {}

StyleParams::StyleParams()
    : buttonRadius(6), inputRadius(6), scrollbarWidth(8), spacing(8),
      touchTarget(32), scrollbarMode(AlwaysFaint), buttonStyle(Flat),
      compositingMode(ColorBlend) {}

// ========== Color tables ==========

StyleParams StyleParams::qtFusion(bool dark) {
    StyleParams p;
    auto& c = dark ? p.dark : p.light;
    if (dark) {
        c.windowBg       = QColor("#0d1117");
        c.surfaceBg      = QColor("#21262d");
        c.baseBg         = QColor("#161b22");
        c.hoverBg        = QColor("#30363d");
        c.activeBg       = QColor("#3d444d");
        c.textPrimary    = QColor("#c9d1d9");
        c.textMuted      = QColor("#8b949e");
        c.textDisabled   = QColor("#484f58");
        c.accent         = QColor("#1f6feb");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#30363d");
        c.borderFocus    = QColor("#1f6feb");
        c.scrollbarSlider= QColor("#30363d");
        c.scrollbarHover = QColor("#3d444d");
    } else {
        c.windowBg       = QColor("#f0f0f0");
        c.surfaceBg      = QColor("#ffffff");
        c.baseBg         = QColor("#ffffff");
        c.hoverBg        = QColor("#e5e5e5");
        c.activeBg       = QColor("#d0d0d0");
        c.textPrimary    = QColor("#1a1a1a");
        c.textMuted      = QColor("#666666");
        c.textDisabled   = QColor("#999999");
        c.accent         = QColor("#0969da");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#c0c0c0");
        c.borderFocus    = QColor("#0969da");
        c.scrollbarSlider= QColor("#c0c0c0");
        c.scrollbarHover = QColor("#a0a0a0");
    }
    p.buttonRadius    = 6;
    p.inputRadius     = 6;
    p.scrollbarWidth  = 8;
    p.spacing         = 8;
    p.touchTarget     = 32;
    p.scrollbarMode   = StyleParams::AlwaysFaint;
    p.buttonStyle     = StyleParams::Flat;
    p.compositingMode = ColorBlend;
    return p;
}

StyleParams StyleParams::macOS(bool dark) {
    StyleParams p;
    auto& c = dark ? p.dark : p.light;
    if (dark) {
        c.windowBg       = QColor("#323232");
        c.surfaceBg      = QColor("#3a3a3c");
        c.baseBg         = QColor("#1e1e1e");
        c.hoverBg        = QColor("#48484a");
        c.activeBg       = QColor("#555557");
        c.textPrimary    = QColor("#ffffff");
        c.textMuted      = QColor("#8e8e93");
        c.textDisabled   = QColor("#636366");
        c.accent         = QColor("#007aff");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#48484a");
        c.borderFocus    = QColor("#007aff");
        c.scrollbarSlider= QColor("#8e8e93");
        c.scrollbarHover = QColor("#a0a0a5");
    } else {
        c.windowBg       = QColor("#ededed");
        c.surfaceBg      = QColor("#ffffff");
        c.baseBg         = QColor("#ffffff");
        c.hoverBg        = QColor("#e8e8e8");
        c.activeBg       = QColor("#d4d4d4");
        c.textPrimary    = QColor("#1d1d1f");
        c.textMuted      = QColor("#6c6c70");
        c.textDisabled   = QColor("#a1a1a6");
        c.accent         = QColor("#007aff");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#d2d2d7");
        c.borderFocus    = QColor("#007aff");
        c.scrollbarSlider= QColor("#c6c6c8");
        c.scrollbarHover = QColor("#a1a1a3");
    }
    p.buttonRadius    = 8;
    p.inputRadius     = 8;
    p.scrollbarWidth  = 4;
    p.spacing         = 8;
    p.touchTarget     = 32;
    p.scrollbarMode   = StyleParams::OverlayFade;
    p.buttonStyle     = StyleParams::Capsule;
    p.compositingMode = AlphaBlend;
    return p;
}

StyleParams StyleParams::windows11(bool dark) {
    StyleParams p;
    auto& c = dark ? p.dark : p.light;
    if (dark) {
        c.windowBg       = QColor("#202020");
        c.surfaceBg      = QColor("#2c2c2c");
        c.baseBg         = QColor("#1f1f1f");
        c.hoverBg        = QColor("#3d3d3d");
        c.activeBg       = QColor("#484848");
        c.textPrimary    = QColor("#ffffff");
        c.textMuted      = QColor("#a0a0a0");
        c.textDisabled   = QColor("#5a5a5a");
        c.accent         = QColor("#60cdff");
        c.accentText     = QColor("#000000");
        c.border         = QColor("#555555");
        c.borderFocus    = QColor("#60cdff");
        c.scrollbarSlider= QColor("#555555");
        c.scrollbarHover = QColor("#757575");
    } else {
        c.windowBg       = QColor("#f9f9f9");
        c.surfaceBg      = QColor("#ffffff");
        c.baseBg         = QColor("#ffffff");
        c.hoverBg        = QColor("#eaeaea");
        c.activeBg       = QColor("#d6d6d6");
        c.textPrimary    = QColor("#1a1a1a");
        c.textMuted      = QColor("#616161");
        c.textDisabled   = QColor("#a0a0a0");
        c.accent         = QColor("#0078d4");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#d2d2d2");
        c.borderFocus    = QColor("#0078d4");
        c.scrollbarSlider= QColor("#c0c0c0");
        c.scrollbarHover = QColor("#a0a0a0");
    }
    p.buttonRadius    = 4;
    p.inputRadius     = 4;
    p.scrollbarWidth  = 7;
    p.spacing         = 8;
    p.touchTarget     = 32;
    p.scrollbarMode   = StyleParams::AlwaysFaint;
    p.buttonStyle     = StyleParams::Border;
    p.compositingMode = ColorBlend;
    return p;
}

StyleParams StyleParams::materialYou(bool dark) {
    StyleParams p;
    auto& c = dark ? p.dark : p.light;
    if (dark) {
        c.windowBg       = QColor("#1c1b1f");
        c.surfaceBg      = QColor("#2b2930");
        c.baseBg         = QColor("#1c1b1f");
        c.hoverBg        = QColor("#3b3940");
        c.activeBg       = QColor("#48464d");
        c.textPrimary    = QColor("#e6e1e5");
        c.textMuted      = QColor("#cac4d0");
        c.textDisabled   = QColor("#938f99");
        c.accent         = QColor("#d0bcff");
        c.accentText     = QColor("#381e72");
        c.border         = QColor("#938f99");
        c.borderFocus    = QColor("#d0bcff");
        c.scrollbarSlider= QColor("#938f99");
        c.scrollbarHover = QColor("#cac4d0");
    } else {
        c.windowBg       = QColor("#fffbfe");
        c.surfaceBg      = QColor("#f3edf7");
        c.baseBg         = QColor("#fffbfe");
        c.hoverBg        = QColor("#e8e3ec");
        c.activeBg       = QColor("#dbd6e0");
        c.textPrimary    = QColor("#1c1b1f");
        c.textMuted      = QColor("#79747e");
        c.textDisabled   = QColor("#cac4d0");
        c.accent         = QColor("#6750a4");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#79747e");
        c.borderFocus    = QColor("#6750a4");
        c.scrollbarSlider= QColor("#cac4d0");
        c.scrollbarHover = QColor("#49454f");
    }
    p.buttonRadius    = 16;
    p.inputRadius     = 12;
    p.scrollbarWidth  = 4;
    p.spacing         = 16;
    p.touchTarget     = 44;
    p.scrollbarMode   = StyleParams::OverlayFade;
    p.buttonStyle     = StyleParams::Capsule;
    p.compositingMode = ColorBlend;
    return p;
}

StyleParams StyleParams::make(StyleId id, bool dark) {
    switch (id) {
        case StyleQtFusion:  return qtFusion(dark);
        case StyleMacOS:     return macOS(dark);
        case StyleWindows:   return windows11(dark);
        case StyleMaterial:  return materialYou(dark);
    }
    return qtFusion(dark);
}

// ========== Helpers ==========

QColor lerpColor(const QColor& a, const QColor& b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
#ifdef QT3_BUILD
    return QColor(
        a.red()   + (int)((b.red()   - a.red())   * t),
        a.green() + (int)((b.green() - a.green()) * t),
        a.blue()  + (int)((b.blue()  - a.blue())  * t)
    );
#else
    return QColor(
        a.red()   + (int)((b.red()   - a.red())   * t),
        a.green() + (int)((b.green() - a.green()) * t),
        a.blue()  + (int)((b.blue()  - a.blue())  * t),
        a.alpha() + (int)((b.alpha() - a.alpha()) * t)
    );
#endif
}

static QColor safeColor(const QColor& c) {
    return c.isValid() ? c : QColor("#0d1117");
}

static QColor compositeColor(const QColor& bg, const QColor& fg, float ratio,
                             CompositingMode mode) {
    switch (mode) {
        case AlphaBlend: {
#ifdef QT3_BUILD
            return fg;
#else
            QColor c = fg;
            c.setAlphaF(ratio);
            return c;
#endif
        }
        case ColorBlend:
            return lerpColor(bg, fg, ratio);
        case JumpCut:
            return fg;
    }
    return fg;
}

static bool isGlobalDark() {
    return ThemeManager::isDarkMode();
}

const StyleParams::Palette& currentPalette() {
    if (!g_activeParams) {
        static StyleParams::Palette fallback;
        return fallback;
    }
    return isGlobalDark() ? g_activeParams->dark : g_activeParams->light;
}

StyleParams makeCurrentParams() {
    if (!g_activeParams) return StyleParams::qtFusion(isGlobalDark());
    return *g_activeParams;
}

static QColor buttonColor(const StyleParams::Palette& pal, const StyleParams& params,
                          bool hover, bool down, bool disabled) {
    if (disabled) return pal.windowBg;
    if (down) return compositeColor(pal.windowBg, pal.activeBg, 1.0f, params.compositingMode);
    if (hover) return compositeColor(pal.windowBg, pal.hoverBg, 1.0f, params.compositingMode);
    return compositeColor(pal.windowBg, pal.surfaceBg, 1.0f, params.compositingMode);
}

static int buttonRadiusFor(const StyleParams& params, const QRect& r) {
    if (params.buttonStyle == StyleParams::Capsule)
        return r.height() / 2;
    return params.buttonRadius;
}

static QColor disabledBlend(const QColor& enabled, const QColor& disabled,
                            CompositingMode mode) {
    switch (mode) {
        case AlphaBlend: {
#ifdef QT3_BUILD
            return lerpColor(enabled, disabled, 0.0f);
#else
            QColor c = enabled;
            c.setAlphaF(0.4f);
            return c;
#endif
        }
        case ColorBlend:
            return lerpColor(disabled, enabled, 0.4f);
        case JumpCut:
            return disabled;
    }
    return enabled;
}

// ========== Constructor ==========

LimeStyle::LimeStyle() :
#ifdef QT3_BUILD
    QStyle()
#else
    QProxyStyle()
#endif
{
#ifdef QT3_BUILD
    m_baseStyle = &QApplication::style();
#endif
}

// ========== drawPrimitive ==========

#ifdef QT3_BUILD

void LimeStyle::drawPrimitive(PrimitiveElement pe, QPainter *p, const QRect &r,
                              const QColorGroup &cg, SFlags flags,
                              const QStyleOption &opt) const {
    const StyleParams::Palette& pal = currentPalette();
    const StyleParams& params = makeCurrentParams();
    CompositingMode mode = params.compositingMode;

    switch (pe) {
    case PE_PanelLineEdit: {
        p->save();
        int rad = params.inputRadius;
        QColor bg = pal.baseBg;
        p->setBrush(bg);
        p->setPen(Qt::NoPen);
        p->drawRoundRect(QRect(r.x()+1, r.y()+1, r.width()-2, r.height()-2),
                         rad * 200 / r.width(), rad * 200 / r.height());
        QColor borderColor = (flags & Style_HasFocus) ? pal.borderFocus : pal.border;
        QPen borderPen(borderColor, 1);
        p->setPen(borderPen);
        p->setBrush(Qt::NoBrush);
        p->drawRoundRect(r, rad * 200 / r.width(), rad * 200 / r.height());
        p->restore();
        return;
    }
    case PE_FocusRect:
        return;
    case PE_ButtonCommand: {
        bool hover = (flags & Style_MouseOver);
        bool down = (flags & Style_Down);
        bool disabled = !(flags & Style_Enabled);
        QColor bg = buttonColor(pal, params, hover, down, disabled);
        p->save();
        p->setBrush(bg);
        p->setPen(Qt::NoPen);
        int rad = buttonRadiusFor(params, r);
        if (rad > 0)
            p->drawRoundRect(r, rad * 200 / r.width(), rad * 200 / r.height());
        else
            p->drawRect(r);
        p->restore();
        return;
    }
    default:
        break;
    }

    m_baseStyle->drawPrimitive(pe, p, r, cg, flags, opt);
}

#else // Qt4

void LimeStyle::drawPrimitive(PrimitiveElement pe, const QStyleOption *opt,
                              QPainter *p, const QWidget *w) const {
    const StyleParams::Palette& pal = currentPalette();
    const StyleParams& params = makeCurrentParams();
    CompositingMode mode = params.compositingMode;

    switch (pe) {
    case PE_PanelLineEdit:
    case PE_FrameLineEdit: {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QRect r = opt->rect;
        int rad = params.inputRadius;
        p->setBrush(pal.baseBg);
        p->setPen(Qt::NoPen);
        p->drawRoundedRect(r.adjusted(1, 1, -1, -1), rad, rad);
        bool hasFocus = opt->state & State_HasFocus;
        QColor borderColor = hasFocus ? pal.borderFocus : pal.border;
        QPen borderPen(borderColor, 1);
        p->setPen(borderPen);
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
        p->restore();
        return;
    }
#ifdef QT3_BUILD
    case PE_FocusRect:
#endif
    case PE_FrameFocusRect:
        return;
#ifdef QT3_BUILD
    case PE_ButtonCommand:
#endif
    case PE_PanelButtonCommand: {
        bool hover = opt->state & State_MouseOver;
        bool down = opt->state & State_Sunken;
        bool disabled = !(opt->state & State_Enabled);
        QColor bg = buttonColor(pal, params, hover, down, disabled);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setBrush(bg);
        p->setPen(Qt::NoPen);
        QRect r = opt->rect;
        int rad = buttonRadiusFor(params, r);
        p->drawRoundedRect(r, rad, rad);
        p->restore();
        return;
    }
    default:
        break;
    }

    QProxyStyle::drawPrimitive(pe, opt, p, w);
}

#endif

// ========== drawControl ==========

#ifdef QT3_BUILD

void LimeStyle::drawControl(ControlElement ce, QPainter *p, const QWidget *widget,
                            const QRect &r, SFlags flags,
                            const QStyleOption &opt) const {
    const StyleParams::Palette& pal = currentPalette();
    const StyleParams& params = makeCurrentParams();
    CompositingMode mode = params.compositingMode;

    switch (ce) {
    case CE_PushButton: {
        bool hover = (flags & Style_MouseOver);
        bool down = (flags & Style_Down);
        bool disabled = !(flags & Style_Enabled);
        QColor bg = buttonColor(pal, params, hover, down, disabled);

        p->save();
        p->setBrush(bg);
        p->setPen(pal.border);
        int rad = buttonRadiusFor(params, r);
        if (rad > 0)
            p->drawRoundRect(r, rad * 200 / r.width(), rad * 200 / r.height());
        else
            p->drawRect(r);
        p->restore();
        return;
    }
    case CE_PopupMenuItem: {
        if (flags & Style_Selected) {
            p->save();
            p->fillRect(r, pal.accent);
            p->restore();
        }
        break;
    }
    case CE_CheckBox:
    case CE_CheckBoxLabel:
    default:
        break;
    }

    {
        QColorGroup cg = widget ? widget->colorGroup() : QApplication::palette().active();
        m_baseStyle->drawControl(ce, p, widget, r, cg, flags, opt);
    }
}

#else // Qt4

void LimeStyle::drawControl(ControlElement ce, const QStyleOption *opt,
                            QPainter *p, const QWidget *w) const {
    const StyleParams::Palette& pal = currentPalette();
    const StyleParams& params = makeCurrentParams();
    CompositingMode mode = params.compositingMode;

    switch (ce) {
    case CE_PushButton: {
        bool hover = opt->state & State_MouseOver;
        bool down = opt->state & State_Sunken;
        bool disabled = !(opt->state & State_Enabled);
        QColor bg = buttonColor(pal, params, hover, down, disabled);
        QColor fg = disabled
                    ? pal.textDisabled
                    : (down ? pal.accentText : pal.textPrimary);

        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QRect r = opt->rect;
        p->setBrush(bg);
        p->setPen(Qt::NoPen);
        int rad = buttonRadiusFor(params, r);
        p->drawRoundedRect(r, rad, rad);
        p->setPen(fg);
        if (const QStyleOptionButton* bopt =
            qstyleoption_cast<const QStyleOptionButton*>(opt)) {
            p->drawText(r, Qt::AlignCenter, bopt->text);
        }
        p->restore();
        return;
    }
#ifdef QT3_BUILD
    case CE_PopupMenuItem:
#endif
    case CE_MenuItem: {
        bool selected = opt->state & State_Selected;
        bool separator = false;
        bool enabled = opt->state & State_Enabled;
        QString text;

        if (const QStyleOptionMenuItem* mopt =
            qstyleoption_cast<const QStyleOptionMenuItem*>(opt)) {
            separator = mopt->menuItemType == QStyleOptionMenuItem::Separator;
            text = mopt->text;
            enabled = mopt->state & State_Enabled;
        }

        if (separator) {
            p->save();
            p->setPen(pal.border);
            p->drawLine(opt->rect.left() + 8, opt->rect.center().y(),
                         opt->rect.right() - 8, opt->rect.center().y());
            p->restore();
            return;
        }

        p->save();
        if (selected) {
            p->fillRect(opt->rect, pal.accent);
        } else {
            p->fillRect(opt->rect, pal.windowBg);
        }
        QColor fg = selected ? pal.accentText
                    : (enabled ? pal.textPrimary : pal.textDisabled);
        p->setPen(fg);
        QRect textR = opt->rect.adjusted(16, 0, -16, 0);
        int ampPos = text.indexOf(QLatin1Char('&'));
        if (ampPos >= 0) text.remove(ampPos, 1);
        p->drawText(textR, Qt::AlignVCenter | Qt::AlignLeft, text);
        p->restore();
        return;
    }
    case CE_CheckBox:
    case CE_CheckBoxLabel:
        break;
    case CE_ItemViewItem: {
        if (const QStyleOptionViewItemV4* vopt =
            qstyleoption_cast<const QStyleOptionViewItemV4*>(opt)) {
            QRect r = opt->rect;
            bool sel = opt->state & State_Selected;
            bool hover = opt->state & State_MouseOver;

            p->save();
            if (sel) {
                p->fillRect(r, pal.accent);
            } else if (hover) {
                p->fillRect(r, pal.hoverBg);
            }

            p->setPen(sel ? pal.accentText : pal.textPrimary);
            QRect textRect = r.adjusted(12, 0, -12, 0);
            p->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, vopt->text);
            p->restore();
            return;
        }
        break;
    }
    default:
        break;
    }

    QProxyStyle::drawControl(ce, opt, p, w);
}

#endif

// ========== drawComplexControl ==========

#ifdef QT3_BUILD

void LimeStyle::drawComplexControl(ComplexControl cc, QPainter *p,
                                   const QWidget *widget, const QRect &r,
                                   const QColorGroup &cg, SFlags flags,
                                   SCFlags controls, SCFlags active,
                                   const QStyleOption &opt) const {
    const StyleParams::Palette& pal = currentPalette();
    const StyleParams& params = makeCurrentParams();
    CompositingMode mode = params.compositingMode;

    switch (cc) {
    case CC_ScrollBar: {
        QRect sliderRect = querySubControlMetrics(CC_ScrollBar, widget,
                                                  SC_ScrollBarSlider, opt);
        if (sliderRect.isValid()) {
            p->save();
            p->setBrush(pal.scrollbarSlider);
            p->setPen(Qt::NoPen);
            p->drawRoundRect(sliderRect, 4, 4);
            p->restore();
        }
        return;
    }
    case CC_ComboBox: {
        bool hover = (flags & Style_MouseOver);
        bool down = (flags & Style_Down);
        bool disabled = !(flags & Style_Enabled);
        QColor bg = buttonColor(pal, params, hover, down, disabled);
        QColor fg = disabled ? pal.textDisabled : pal.textPrimary;

        p->save();
        p->setBrush(bg);
        p->setPen(pal.border);
        int rad = buttonRadiusFor(params, r);
        p->drawRoundRect(r, rad * 200 / r.width(), rad * 200 / r.height());

        QRect arrowRect = querySubControlMetrics(CC_ComboBox, widget,
                                                 SC_ComboBoxArrow, opt);
        bool popupOpen = (active & SC_ComboBoxArrow);
        QString arrow = popupOpen ? QString(QChar(0x25B2)) : QString(QChar(0x25BC));
        p->setPen(fg);
        p->drawText(arrowRect, Qt::AlignCenter, arrow);
        p->restore();
        return;
    }
    default:
        break;
    }

    m_baseStyle->drawComplexControl(cc, p, widget, r, cg, flags,
                                     controls, active, opt);
}

#else // Qt4

void LimeStyle::drawComplexControl(ComplexControl cc,
                                   const QStyleOptionComplex *opt,
                                   QPainter *p, const QWidget *w) const {
    const StyleParams::Palette& pal = currentPalette();
    const StyleParams& params = makeCurrentParams();
    CompositingMode mode = params.compositingMode;

    switch (cc) {
    case CC_ScrollBar: {
        QRect sliderRect = subControlRect(CC_ScrollBar, opt,
                                          SC_ScrollBarSlider, w);
        if (sliderRect.isValid()) {
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setBrush(pal.scrollbarSlider);
            p->setPen(Qt::NoPen);
            p->drawRoundedRect(sliderRect, 4, 4);
            p->restore();
        }
        return;
    }
    case CC_ComboBox: {
        bool hover = opt->state & State_MouseOver;
        bool down = opt->state & State_Sunken;
        bool disabled = !(opt->state & State_Enabled);
        QColor bg = buttonColor(pal, params, hover, down, disabled);
        QColor fg = disabled ? pal.textDisabled : pal.textPrimary;

        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QRect r = opt->rect;
        p->setBrush(bg);
        p->setPen(pal.border);
        int rad = buttonRadiusFor(params, r);
        p->drawRoundedRect(r, rad, rad);

        QRect arrowRect = subControlRect(CC_ComboBox, opt,
                                         SC_ComboBoxArrow, w);
        bool popupOpen = opt->state & State_On;
        QString arrow = popupOpen ? QString(QChar(0x25B2)) : QString(QChar(0x25BC));
        p->setPen(fg);
        p->drawText(arrowRect, Qt::AlignCenter, arrow);
        p->restore();
        return;
    }
    default:
        break;
    }

    QProxyStyle::drawComplexControl(cc, opt, p, w);
}

#endif

// ========== pixelMetric ==========

#ifdef QT3_BUILD

int LimeStyle::pixelMetric(PixelMetric m, const QWidget *w) const {
    const StyleParams& params = makeCurrentParams();

    switch (m) {
    case PM_ScrollBarExtent: return params.scrollbarWidth;
    case PM_ButtonMargin:    return 6;
    case PM_DefaultFrameWidth: return 1;
    default: break;
    }

    return m_baseStyle->pixelMetric(m, w);
}

#else

int LimeStyle::pixelMetric(PixelMetric m, const QStyleOption *opt,
                            const QWidget *w) const {
    const StyleParams& params = makeCurrentParams();

    switch (m) {
    case PM_ScrollBarExtent: return params.scrollbarWidth;
    case PM_ButtonMargin:    return 6;
    case PM_DefaultFrameWidth: return 1;
    default: break;
    }

    return QProxyStyle::pixelMetric(m, opt, w);
}

#endif

// ========== styleHint ==========

#ifdef QT3_BUILD

int LimeStyle::styleHint(StyleHint sh, const QStyleOption &opt,
                         const QWidget *w, QStyleHintReturn *hret) const {
    const StyleParams& params = makeCurrentParams();

    switch (sh) {
    default: break;
    }

    return m_baseStyle->styleHint(sh, w, opt, hret);
}

#else

int LimeStyle::styleHint(StyleHint sh, const QStyleOption *opt,
                         const QWidget *w, QStyleHintReturn *hret) const {
    const StyleParams& params = makeCurrentParams();

    switch (sh) {
    default: break;
    }

    return QProxyStyle::styleHint(sh, opt, w, hret);
}

#endif
