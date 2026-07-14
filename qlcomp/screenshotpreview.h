#ifndef SCREENSHOT_PREVIEW_H
#define SCREENSHOT_PREVIEW_H

// Reference:
//   PhotoViewer (existing): qltox/photoviewer.h
//   QDialog pattern: qlcomp/ConfigDialog.h

#include "compat34.h"
#include <qdialog.h>
#include <qpixmap.h>
#include <qstring.h>
#include <qlabel.h>
#include <qpushbutton.h>

class PreviewDialog : public QDialog {
    Q_OBJECT
public:
    PreviewDialog(const QString& filePath, QWidget* parent = nullptr);
    ~PreviewDialog();

signals:
    void sendRequested(const QString& filePath);
    void saveRequested(const QString& filePath);
    void cancelled();

private slots:
    void onSendClicked();
    void onSaveClicked();
    void onCancelClicked();

private:
    void setupUi();

    QString m_filePath;
    QPixmap m_pixmap;
    QLabel* m_imageLabel;
    QPushButton* m_sendBtn;
    QPushButton* m_saveBtn;
    QPushButton* m_cancelBtn;
};

#endif
