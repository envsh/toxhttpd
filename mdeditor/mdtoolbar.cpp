#include "mdtoolbar.h"
#include <qlayout.h>
#include <qdatetime.h>
#include <qcursor.h>

#ifdef QT3_BUILD
#include <qtextedit.h>
#else
#include <QTextEdit>
#endif

MdToolbar::MdToolbar(QWidget* parent)
    : QWidget(parent), m_editor(0) {
    QHBoxLayout* lay = new QHBoxLayout(this);
    lay->setMargin(2);
    lay->setSpacing(2);

    lay->addWidget(makeBtn("R"));
    lay->addWidget(makeBtn("Y"));
    lay->addSpacing(8);

    QPushButton* hBtn = makeBtn("H");
    lay->addWidget(hBtn);

#ifdef QT3_BUILD
    m_headingMenu = new QPopupMenu(hBtn);
    for (int i = 1; i <= 6; ++i) {
        m_headingMenu->insertItem(QString("H%1").arg(i), i - 1);
    }
#else
    m_headingMenu = new QMenu(hBtn);
    for (int i = 1; i <= 6; ++i) {
        m_headingMenu->addAction(QString("H%1").arg(i));
    }
#endif
    connect(hBtn, SIGNAL(clicked()), this, SLOT(onHeadingClicked()));

    lay->addWidget(makeBtn("B"));
    lay->addWidget(makeBtn("I"));
    lay->addWidget(makeBtn("U"));
    lay->addWidget(makeBtn("S"));
    lay->addWidget(makeBtn("C"));
    lay->addWidget(makeBtn("CB"));
    lay->addSpacing(8);

    lay->addWidget(makeBtn("Q"));
    lay->addWidget(makeBtn("UL"));
    lay->addWidget(makeBtn("OL"));
    lay->addWidget(makeBtn("TL"));
    lay->addWidget(makeBtn("---"));
    lay->addSpacing(8);

    lay->addWidget(makeBtn("L"));
    lay->addWidget(makeBtn("IM"));
    lay->addWidget(makeBtn("T"));
    lay->addWidget(makeBtn("D"));
    lay->addWidget(makeBtn("FN"));
    lay->addWidget(makeBtn("TOC"));
    lay->addSpacing(8);

    QPushButton* previewBtn = makeBtn("P");
    lay->addWidget(previewBtn);
    connect(previewBtn, SIGNAL(clicked()), this, SIGNAL(togglePreview()));

    lay->addStretch();
}

QPushButton* MdToolbar::makeBtn(const QString& text) {
    QPushButton* btn = new QPushButton(text, this);
    btn->setFixedSize(32, 24);
    btn->setFlat(true);
    connect(btn, SIGNAL(clicked()), this, SLOT(onButtonClicked()));
    return btn;
}

void MdToolbar::setEditor(QTextEdit* editor) {
    m_editor = editor;
}

void MdToolbar::onButtonClicked() {
    QPushButton* btn = (QPushButton*)sender();
    if (!btn) {
        return;
    }
    QString id = btn->text();

    if (id == "R") {
        emit undoRequested();
    } else if (id == "Y") {
        emit redoRequested();
    } else if (id == "B") {
        emit insertBold();
    } else if (id == "I") {
        emit insertItalic();
    } else if (id == "U") {
        emit insertUnderline();
    } else if (id == "S") {
        emit insertStrikethrough();
    } else if (id == "C") {
        emit insertInlineCode();
    } else if (id == "CB") {
        emit insertCodeBlock();
    } else if (id == "Q") {
        emit insertQuote();
    } else if (id == "UL") {
        emit insertUl();
    } else if (id == "OL") {
        emit insertOl();
    } else if (id == "TL") {
        emit insertTodo();
    } else if (id == "---") {
        emit insertHr();
    } else if (id == "L") {
        emit insertLink();
    } else if (id == "IM") {
        emit insertImage();
    } else if (id == "T") {
        emit insertTable();
    } else if (id == "D") {
        onDateClicked();
    } else if (id == "FN") {
        emit insertFootnote();
    } else if (id == "TOC") {
        emit insertToc();
    }
}

void MdToolbar::onHeadingClicked() {
#ifdef QT3_BUILD
    int id = m_headingMenu->exec(QCursor::pos());
    if (id >= 0) {
        emit insertHeading(id + 1);
    }
#else
    QAction* act = m_headingMenu->exec(QCursor::pos());
    if (act) {
        QString text = act->text();
        int level = text.mid(1).toInt();
        if (level >= 1 && level <= 6) {
            emit insertHeading(level);
        }
    }
#endif
}

void MdToolbar::onDateClicked() {
    emit insertDate();
}
