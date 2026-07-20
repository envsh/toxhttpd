#include "pushhandler.h"
#include <QDebug>

#if defined(Q_OS_ANDROID)

#include <jni.h>
#include <QCoreApplication>
#include <QJniObject>
#include <QSettings>
#include <QUuid>

static PushHandler* s_instance = nullptr;

void PushHandler::start()
{
    if (s_instance) return;
    s_instance = new PushHandler();

    QSettings s;
    QString deviceToken = s.value("pushDeviceToken").toString();
    if (deviceToken.isEmpty()) {
        deviceToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.setValue("pushDeviceToken", deviceToken);
        s.sync();
    }

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([deviceToken]() {
        auto ctx = QNativeInterface::QAndroidApplication::context();

        // 共享 topic（所有设备）
        QJniObject::callStaticMethod<void>(
            "org/unifiedpush/android/connector/UnifiedPush",
            "register",
            "(Landroid/content/Context;Ljava/lang/String;)V",
            ctx.object(),
            QJniObject::fromString("qsktox").object());

        // per-device topic
        QJniObject::callStaticMethod<void>(
            "org/unifiedpush/android/connector/UnifiedPush",
            "register",
            "(Landroid/content/Context;Ljava/lang/String;)V",
            ctx.object(),
            QJniObject::fromString(deviceToken).object());

        qDebug() << "[PushHandler] registered: shared=qsktox device=" << deviceToken;
    });
}

void PushHandler::stop()
{
    if (!s_instance) return;

    QSettings s;
    QString deviceToken = s.value("pushDeviceToken").toString();

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([deviceToken]() {
        auto ctx = QNativeInterface::QAndroidApplication::context();

        // unregister 共享
        QJniObject::callStaticMethod<void>(
            "org/unifiedpush/android/connector/UnifiedPush",
            "unregister",
            "(Landroid/content/Context;Ljava/lang/String;)V",
            ctx.object(),
            QJniObject::fromString("qsktox").object());

        // unregister per-device
        if (!deviceToken.isEmpty()) {
            QJniObject::callStaticMethod<void>(
                "org/unifiedpush/android/connector/UnifiedPush",
                "unregister",
                "(Landroid/content/Context;Ljava/lang/String;)V",
                ctx.object(),
                QJniObject::fromString(deviceToken).object());
        }

        qDebug() << "[PushHandler] unregistered both";
    });

    delete s_instance;
    s_instance = nullptr;
}

static QString jstringToQString(JNIEnv* env, jstring js)
{
    if (!js) return {};
    const char* raw = env->GetStringUTFChars(js, nullptr);
    QString s = QString::fromUtf8(raw);
    env->ReleaseStringUTFChars(js, raw);
    return s;
}

extern "C" JNIEXPORT void JNICALL
Java_io_fedlet_mobutil_PushServiceImpl_onNewEndpointNative(
    JNIEnv* env, jobject /*thiz*/, jstring jEndpoint, jstring jInstance)
{
    QString endpoint = jstringToQString(env, jEndpoint);
    QString instance = jstringToQString(env, jInstance);
    qDebug() << "[PushHandler] new endpoint:" << endpoint << "instance:" << instance;

    // 只保存 per-device endpoint（共享 "qsktox" 的不保存）
    if (instance != "qsktox") {
        QSettings s;
        s.setValue("pushDeviceEndpoint", endpoint);
        s.sync();
    }

    if (s_instance) {
        QMetaObject::invokeMethod(s_instance, [endpoint, instance]() {
            emit s_instance->pushReceived(endpoint, instance);
        }, Qt::QueuedConnection);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_io_fedlet_mobutil_PushServiceImpl_onMessageNative(
    JNIEnv* env, jobject /*thiz*/, jbyteArray jMessage, jstring jInstance)
{
    QByteArray message;
    if (jMessage) {
        jsize len = env->GetArrayLength(jMessage);
        message.resize(len);
        env->GetByteArrayRegion(jMessage, 0, len,
            reinterpret_cast<jbyte*>(message.data()));
    }
    QString instance = jstringToQString(env, jInstance);
    qDebug() << "[PushHandler] message received, size:" << message.size()
             << "instance:" << instance;

    if (s_instance) {
        QMetaObject::invokeMethod(s_instance, [message, instance]() {
            emit s_instance->pushMessage(message, instance);
        }, Qt::QueuedConnection);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_io_fedlet_mobutil_PushServiceImpl_onRegistrationFailedNative(
    JNIEnv* env, jobject /*thiz*/, jstring jReason, jstring jInstance)
{
    QString reason = jstringToQString(env, jReason);
    QString instance = jstringToQString(env, jInstance);
    qWarning() << "[PushHandler] registration failed:" << reason
               << "instance:" << instance;
}

extern "C" JNIEXPORT void JNICALL
Java_io_fedlet_mobutil_PushServiceImpl_onUnregisteredNative(
    JNIEnv* env, jobject /*thiz*/, jstring jInstance)
{
    QString instance = jstringToQString(env, jInstance);
    qDebug() << "[PushHandler] unregistered, instance:" << instance;
}

#else

void PushHandler::start() {}
void PushHandler::stop() {}

#endif
