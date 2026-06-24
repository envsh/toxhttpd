#include "generic_slot.h"

#ifdef QT3_BUILD
#include <qmetaobject.h>
bool GenericSlot::qt_invoke(int id, QUObject* o) {
    int adjusted = id - GenericSlotBase::staticMetaObject()->slotOffset();
    if (adjusted == 0) {
        m_fn(o);
        return TRUE;
    }
    return GenericSlotBase::qt_invoke(id, o);
}
#else
int GenericSlot::qt_metacall(QMetaObject::Call c, int id, void** args) {
    id = QObject::qt_metacall(c, id, args);
    if (id < 0 || c != QMetaObject::InvokeMetaMethod)
        return id;
    m_fn(args);
    return -1;
}
#endif
