#include "photoviewer.h"
#include "compat34.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#ifdef QT3_BUILD
#include <qfiledialog.h>
#else
#include <QFileDialog>
#endif

PhotoViewer::PhotoViewer(QWidget* parent, const QPixmap& pixmap)
    : QDialog(parent), m_pixmap(pixmap)
{
    qSetWindowTitle(this, qFromUtf8("PhotoViewer") + "  —  "
        + QString::number(pixmap.width()) + " × " + QString::number(pixmap.height()));
    resize(800, 600);

    QVBoxLayout* lay = new QVBoxLayout(this);
    QLabel* label = new QLabel(this);
    label->setPixmap(pixmap);
    label->setAlignment(Qt::AlignCenter);
    lay->addWidget(label, 1);

    QPushButton* saveBtn = new QPushButton(qFromUtf8("保存"), this);
    connect(saveBtn, SIGNAL(clicked()), this, SLOT(onSave()));
    QHBoxLayout* btnLay = new QHBoxLayout();
    btnLay->addStretch();
    btnLay->addWidget(saveBtn);
    lay->addLayout(btnLay);
}

void PhotoViewer::onSave() {
#ifdef QT3_BUILD
    QString path = QFileDialog::getSaveFileName(
        qGetHomePath(), qFromUtf8("Images (*.png *.jpg)"), this);
#else
    QString path = QFileDialog::getSaveFileName(
        this, qFromUtf8("保存图片"), qGetHomePath(),
        qFromUtf8("Images (*.png *.jpg)"));
#endif
    if (!path.isEmpty()) {
#ifdef QT3_BUILD
        m_pixmap.save(path, "PNG");
#else
        m_pixmap.save(path);
#endif
    }
}
