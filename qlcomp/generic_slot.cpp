#include "generic_slot.h"

#ifndef QT3_BUILD
int GenericSlot::qt_metacall(QMetaObject::Call c, int id, void** args) {
    id = QObject::qt_metacall(c, id, args);
    if (id < 0 || c != QMetaObject::InvokeMetaMethod)
        return id;
    m_fn(args);
    return -1;
}
#endif
