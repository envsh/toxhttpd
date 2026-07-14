#include "screenshotpreview.h"
#include "compat34.h"
#include "StyleParams.h"

#ifdef QT3_BUILD
#include <qlayout.h>
#include <qfileinfo.h>
#include <qfiledialog.h>
#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qimage.h>
#else
#include <QLayout>
#include <QFileInfo>
#include <QFileDialog>
#include <QApplication>
#include <QDesktopWidget>
#include <QImage>
#endif

ScreenshotPreviewDialog::ScreenshotPreviewDialog(const QString& filePath, QWidget* parent)
#ifdef QT3_BUILD
    : QDialog(parent, "screenshot_preview")
#else
    : QDialog(parent)
#endif
    , m_filePath(filePath)
{
    m_pixmap.load(filePath);

#ifdef QT3_BUILD
    setCaption("Screenshot Preview");
#else
    setWindowTitle("Screenshot Preview");
#endif

    setupUi();
}

ScreenshotPreviewDialog::~ScreenshotPreviewDialog() {
}

void ScreenshotPreviewDialog::setupUi() {
    setMinimumSize(400, 300);
    resize(640, 480);

    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 8, 0);

    m_imageLabel = new QLabel(this);
    if (!m_pixmap.isNull()) {
#ifdef QT3_BUILD
        // Qt3: scale via QImage::smoothScale (QPixmap has no scale method)
        QImage img = m_pixmap.convertToImage();
        int ow = img.width();
        int oh = img.height();
        int mw = 600;
        int mh = 380;
        int dw = QApplication::desktop()->width() - 80;
        int dh = QApplication::desktop()->height() - 160;
        if (dw < mw) { mw = dw; }
        if (dh < mh) { mh = dh; }
        double rw = (double)mw / ow;
        double rh = (double)mh / oh;
        double r = (rw < rh) ? rw : rh;
        if (r < 1.0) {
            QImage scaled = img.smoothScale((int)(ow * r), (int)(oh * r), QImage::ScaleMin);
            QPixmap pm;
            pm.convertFromImage(scaled);
            m_imageLabel->setPixmap(pm);
        } else {
            m_imageLabel->setPixmap(m_pixmap);
        }
#else
        QPixmap scaled = m_pixmap.scaled(600, 380, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_imageLabel->setPixmap(scaled);
#endif
    }
#ifdef QT3_BUILD
    m_imageLabel->setAlignment(AlignCenter);
    m_imageLabel->setBackgroundColor(QColor(32, 32, 32));
#else
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet("background-color: #202020;");
#endif
    mainLayout->addWidget(m_imageLabel, 1);

    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 4);
    mainLayout->addLayout(btnLayout);

    btnLayout->addStretch(1);

    m_sendBtn = new QPushButton("Send", this);
    m_saveBtn = new QPushButton("Save As", this);
    m_cancelBtn = new QPushButton("Cancel", this);

    btnLayout->addWidget(m_sendBtn);
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addWidget(m_cancelBtn);

    connect(m_sendBtn, SIGNAL(clicked()), this, SLOT(onSendClicked()));
    connect(m_saveBtn, SIGNAL(clicked()), this, SLOT(onSaveClicked()));
    connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(onCancelClicked()));
}

void ScreenshotPreviewDialog::onSendClicked() {
    if (!m_filePath.isEmpty()) {
        emit sendRequested(m_filePath);
    }
    close();
}

void ScreenshotPreviewDialog::onSaveClicked() {
    QString savePath;

#ifdef QT3_BUILD
    savePath = QFileDialog::getSaveFileName(QString::null, "PNG Image (*.png)", this);
#else
    savePath = QFileDialog::getSaveFileName(this, "Save Screenshot As",
        QString(), "PNG Image (*.png)");
#endif

    if (!savePath.isEmpty()) {
        m_pixmap.save(savePath, "PNG");
        emit saveRequested(savePath);
    }
}

void ScreenshotPreviewDialog::onCancelClicked() {
    emit cancelled();
    close();
}
