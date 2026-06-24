#ifndef GENERIC_SLOT_BASE_H
#define GENERIC_SLOT_BASE_H

#include "compat34.h"

class GenericSlotBase : public QObject {
    Q_OBJECT
public:
    GenericSlotBase(QObject* parent) : QObject(parent) {}
public slots:
    void call() {}
};

#endif
