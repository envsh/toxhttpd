#include "androidutils.h"
#include <QCoreApplication>

#ifdef Q_OS_ANDROID
#include <QJniObject>

void showAndroidToast(const QString& message) {
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([message]() {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid()) return;
        QJniObject jmsg = QJniObject::fromString(message);
        QJniObject toast = QJniObject::callStaticObjectMethod(
            "android/widget/Toast",
            "makeText",
            "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;",
            context.object(), jmsg.object(), 1);
        if (toast.isValid()) {
            toast.callMethod<void>("show", "()V");
        }
    });
}
#else
void showAndroidToast(const QString& message) {
    Q_UNUSED(message)
}
#endif
