#include "CustomTitleBar.h"
#include "FramelessHelper.h"
#include "translator.h"

CustomTitleBar::CustomTitleBar(QWidget* parent)
    : QWidget(parent) {
    setMinimumHeight(30);
    setMaximumHeight(30);

    QBoxLayout* layout = qNewBoxLayout(this, QBoxLayout::LeftToRight, 1, 1);

    sysBtn = new QPushButton(qFromUtf8("≡"), this);
    sysBtn->setFixedSize(30, 30);
    layout->addWidget(sysBtn, 0);

    titleLabel = new QLabel(_("app_title"), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel, 1);

    minBtn = new QPushButton(qFromUtf8("─"), this);
    minBtn->setFixedSize(30, 30);
    layout->addWidget(minBtn, 0);

    maxBtn = new QPushButton(qFromUtf8("□"), this);
    maxBtn->setFixedSize(30, 30);
    layout->addWidget(maxBtn, 0);

    closeBtn = new QPushButton(qFromUtf8("✕"), this);
    closeBtn->setFixedSize(30, 30);
    layout->addWidget(closeBtn, 0);
}

void CustomTitleBar::connectFramelessHelper(FramelessHelper* helper) {
    if (!helper) return;
    helper->setTitleBar(this);
#ifdef QT3_BUILD
    QObject::connect(closeBtn, SIGNAL(clicked()), helper->parent(), SLOT(close()));
    QObject::connect(minBtn, SIGNAL(clicked()), helper->parent(), SLOT(showMinimized()));
    QObject::connect(maxBtn, SIGNAL(clicked()), helper, SLOT(toggleMaximize()));
#else
    helper->setTitleBarButtons(sysBtn, minBtn, maxBtn, closeBtn);
#endif
}

void CustomTitleBar::setLabel(const QString& text) {
    if (titleLabel) titleLabel->setText(text);
}

QString CustomTitleBar::label() const {
    return titleLabel ? titleLabel->text() : QString();
}
