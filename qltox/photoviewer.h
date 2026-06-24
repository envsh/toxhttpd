#ifndef PHOTOVIEWER_H
#define PHOTOVIEWER_H

#include "compat34.h"
#include <qdialog.h>

class QPixmap;

class PhotoViewer : public QDialog {
    Q_OBJECT
public:
    PhotoViewer(QWidget* parent, const QPixmap& pixmap);
private slots:
    void onSave();
private:
    QPixmap m_pixmap;
};

#endif
