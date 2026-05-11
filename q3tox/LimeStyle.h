#ifndef LIMESTYLE_H
#define LIMESTYLE_H

#include "StyleParams.h"

#ifdef QT3_BUILD
#include <qapplication.h>
#include <qpalette.h>
#include <qstyle.h>
#include <qpainter.h>
#include <qscrollbar.h>
#include <qcombobox.h>
#include <qlineedit.h>
#else
#include <QApplication>
#include <QPalette>
#include <QProxyStyle>
#include <QPainter>
#include <QStyleOption>
#include <QScrollBar>
#include <QComboBox>
#include <QLineEdit>
#include <QStyleOptionViewItemV4>
#endif

QColor lerpColor(const QColor& a, const QColor& b, float t);

const StyleParams::Palette& currentPalette();
StyleParams makeCurrentParams();

#ifdef QT3_BUILD

class LimeStyle : public QStyle {
    // no Q_OBJECT - Qt3 3.5.0 moc can't handle #ifdef inside class body
public:
    LimeStyle();

    void drawPrimitive(PrimitiveElement pe, QPainter *p, const QRect &r,
                       const QColorGroup &cg, SFlags flags = Style_Default,
                       const QStyleOption &opt = QStyleOption()) const;

    void drawControl(ControlElement ce, QPainter *p, const QWidget *widget,
                     const QRect &r, SFlags flags = Style_Default,
                     const QStyleOption &opt = QStyleOption()) const;

    void drawComplexControl(ComplexControl cc, QPainter *p, const QWidget *widget,
                            const QRect &r, const QColorGroup &cg,
                            SFlags flags = Style_Default,
                            SCFlags controls = SC_All,
                            SCFlags active = SC_None,
                            const QStyleOption &opt = QStyleOption()) const;

    int pixelMetric(PixelMetric m, const QWidget *w = 0) const;
    int styleHint(StyleHint sh, const QStyleOption &opt = QStyleOption(),
                  const QWidget *w = 0, QStyleHintReturn *hret = 0) const;

private:
    QStyle* m_baseStyle;
};

#else

class LimeStyle : public QProxyStyle {
public:
    LimeStyle();

    void drawPrimitive(PrimitiveElement pe, const QStyleOption *opt,
                       QPainter *p, const QWidget *w = 0) const;

    void drawControl(ControlElement ce, const QStyleOption *opt,
                     QPainter *p, const QWidget *w = 0) const;

    void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *opt,
                            QPainter *p, const QWidget *w = 0) const;

    int pixelMetric(PixelMetric m, const QStyleOption *opt = 0,
                    const QWidget *w = 0) const;

    int styleHint(StyleHint sh, const QStyleOption *opt = 0,
                  const QWidget *w = 0, QStyleHintReturn *hret = 0) const;
};

#endif

#endif
