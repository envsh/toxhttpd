#include "compat34.h"
#include "plugin_api.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qmessagebox.h>

extern "C" const char* plugin_name(void) {
    return "Dummy Plugin";
}

extern "C" const char* plugin_version(void) {
    return "1.0.0";
}

extern "C" const char* plugin_description(void) {
    return "模板示例插件";
}

extern "C" void* plugin_create(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);
    QVBoxLayout* layout = new QVBoxLayout(w);

    QLabel* lbl = new QLabel("Hello from Dummy Plugin!", w);
    layout->addWidget(lbl);

    QPushButton* btn = new QPushButton("Click Me", w);
    layout->addWidget(btn);

#ifdef QT3_BUILD
    w->setCaption("Dummy Plugin");
#else
    w->setWindowTitle("Dummy Plugin");
#endif
    w->resize(300, 200);
    return w;
}

extern "C" void plugin_destroy(void* widget) {
    delete (QWidget*)widget;
}
