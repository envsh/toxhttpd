#include "pushhandler.h"
#include <QDebug>

#if defined(Q_OS_ANDROID)

#include "androidutils.h"
#include <jni.h>
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QSettings>
#include <QUuid>
#include <QTimerEvent>

static PushHandler* s_instance = nullptr;
static const char* UP_CLASS = "org/unifiedpush/android/connector/UnifiedPush";

static bool checkMethod(const char* methodName, const char* sig)
{
    QJniEnvironment env;
    jclass clazz = env.findClass(UP_CLASS);
    if (!clazz) {
        qWarning() << "[PushHandler] class not found:" << UP_CLASS;
        return false;
    }
    jmethodID mid = env.findStaticMethod(clazz, methodName, sig);
    if (!mid) {
        qWarning() << "[PushHandler] method not found:" << methodName << sig;
        return false;
    }
    return true;
}

PushHandler* PushHandler::instance()
{
    return s_instance;
}

void PushHandler::start()
{
    if (s_instance) return;
    s_instance = new PushHandler();
    QObject::connect(s_instance, &PushHandler::registrationSent,
        s_instance, &PushHandler::startRegistrationTimeout);

    QSettings s;
    QString deviceToken = s.value("pushDeviceToken").toString();
    if (deviceToken.isEmpty()) {
        deviceToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.setValue("pushDeviceToken", deviceToken);
        s.sync();
    }
    qDebug() << "[PushHandler] initialized, device=" << deviceToken;
    showAndroidToast("Push 初始化...");
}

static bool ntfyshPushInstalled = false;

void PushHandler::registerDevice()
{
    if (!s_instance) return;

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
        auto ctx = QNativeInterface::QAndroidApplication::context();

        // 检查是否已有 saved distributor
        QJniObject saved = QJniObject::callStaticMethod<jobject>(
            UP_CLASS,
            "getSavedDistributor",
            "(Landroid/content/Context;)Ljava/lang/String;",
            ctx.object());
        QString savedDistributor = saved.toString();

        // 前置检测：验证是否有 UnifiedPush 分发器（无条件）
        QJniObject ctxObj(ctx.object());
        QJniObject pm = ctxObj.callObjectMethod(
            "getPackageManager", "()Landroid/content/pm/PackageManager;");
        QJniObject pkgName = QJniObject::fromString("io.heckel.ntfy");
        QJniObject packageInfo = pm.callObjectMethod(
            "getPackageInfo",
            "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;",
            pkgName.object(), 0);
        bool hasDistributor = packageInfo.isValid();
        ntfyshPushInstalled = hasDistributor;
        if (!hasDistributor) {
            qWarning() << "[PushHandler] no UnifiedPush distributor found";
            QMetaObject::invokeMethod(s_instance, []() {
                showAndroidToast(QString::fromUtf8("⚠️ 未检测到推送服务，安装 ntfy 可获得消息通知 111"));
            }, Qt::QueuedConnection);
        }

        if (!savedDistributor.isEmpty()) {
            qDebug() << "[PushHandler]已有 distributor:" << savedDistributor;

            // 检测 1：调用前验证 register 方法签名
            const char* registerSig = "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V";
            if (!checkMethod("register", registerSig)) {
                QMetaObject::invokeMethod(s_instance, []() {
                    emit s_instance->registrationFailed("register 方法签名错误");
                }, Qt::QueuedConnection);
                return;
            }

            QJniObject::callStaticMethod<void>(
                UP_CLASS,
                "register",
                registerSig,
                ctx.object(),
                QJniObject::fromString("default").object(),
                QJniObject().object(),
                QJniObject().object());

            // 检测 4：状态验证
            QJniObject verify = QJniObject::callStaticMethod<jobject>(
                UP_CLASS,
                "getSavedDistributor",
                "(Landroid/content/Context;)Ljava/lang/String;",
                ctx.object());
            if (verify.toString() != savedDistributor) {
                qWarning() << "[PushHandler] distributor state changed after register";
                QMetaObject::invokeMethod(s_instance, []() {
                    emit s_instance->registrationFailed("distributor 状态异常");
                }, Qt::QueuedConnection);
                return;
            }

            qDebug() << "[PushHandler] register sent to" << savedDistributor;
            QMetaObject::invokeMethod(s_instance, []() {
                emit s_instance->registrationSent();
            }, Qt::QueuedConnection);
            return;
        }

        // 没有 saved distributor，获取可用列表
        QJniObject list = QJniObject::callStaticMethod<jobject>(
            UP_CLASS,
            "getDistributors",
            "(Landroid/content/Context;)Ljava/util/List;",
            ctx.object());

        // 检测 3：返回值有效性（仅适用于 jobject 返回值）
        if (!list.isValid()) {
            qWarning() << "[PushHandler] getDistributors returned invalid";
            QMetaObject::invokeMethod(s_instance, []() {
                emit s_instance->registrationFailed("getDistributors 调用失败");
            }, Qt::QueuedConnection);
            return;
        }

        QStringList distributors;
        jint size = list.callMethod<jint>("size", "()I");
        for (jint i = 0; i < size; i++) {
            QJniObject item = list.callObjectMethod("get", "(I)Ljava/lang/Object;", i);
            distributors.append(item.toString());
        }
        qDebug() << "[PushHandler] distributors:" << distributors;

        // 回到 Qt 线程处理
        QMetaObject::invokeMethod(s_instance, [distributors]() {
            if (distributors.isEmpty()) {
                qWarning() << "[PushHandler] no distributors found";
                emit s_instance->registrationFailed("未找到 UnifiedPush 分发器，请安装 ntfy 222");
                return;
            }
            if (distributors.size() == 1) {
                s_instance->selectDistributor(distributors.first());
                return;
            }
            emit s_instance->distributorsFound(distributors);
        }, Qt::QueuedConnection);
        QMetaObject::invokeMethod(s_instance, []() {
            emit s_instance->registrationSent();
        }, Qt::QueuedConnection);
    });
}

void PushHandler::selectDistributor(const QString& distributor)
{
    if (!s_instance) return;

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([distributor]() {
        auto ctx = QNativeInterface::QAndroidApplication::context();

        // 检测 1：调用前验证方法
        if (!checkMethod("saveDistributor", "(Landroid/content/Context;Ljava/lang/String;)V")) {
            QMetaObject::invokeMethod(s_instance, []() {
                emit s_instance->registrationFailed("saveDistributor 方法不存在");
            }, Qt::QueuedConnection);
            return;
        }
        const char* registerSig = "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V";
        if (!checkMethod("register", registerSig)) {
            QMetaObject::invokeMethod(s_instance, []() {
                emit s_instance->registrationFailed("register 方法不存在");
            }, Qt::QueuedConnection);
            return;
        }

        // 保存 distributor
        QJniObject::callStaticMethod<void>(
            UP_CLASS,
            "saveDistributor",
            "(Landroid/content/Context;Ljava/lang/String;)V",
            ctx.object(),
            QJniObject::fromString(distributor).object());
        qDebug() << "[PushHandler] saveDistributor:" << distributor;

        // 注册
        QJniObject::callStaticMethod<void>(
            UP_CLASS,
            "register",
            registerSig,
            ctx.object(),
            QJniObject::fromString("default").object(),
            QJniObject().object(),
            QJniObject().object());

        qDebug() << "[PushHandler] register sent to" << distributor;
    });
}

void PushHandler::stop()
{
    if (!s_instance) return;

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
        auto ctx = QNativeInterface::QAndroidApplication::context();

        QJniObject::callStaticMethod<void>(
            UP_CLASS,
            "unregister",
            "(Landroid/content/Context;Ljava/lang/String;)V",
            ctx.object(),
            QJniObject::fromString("default").object());

        qDebug() << "[PushHandler] unregistered";
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

    QSettings s;
    s.setValue("pushDeviceEndpoint", endpoint);
    s.sync();

    if (s_instance) {
        s_instance->cancelRegistrationTimeout();
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

    if (s_instance) {
        s_instance->cancelRegistrationTimeout();
        QMetaObject::invokeMethod(s_instance, [reason]() {
            emit s_instance->registrationFailed(reason);
        }, Qt::QueuedConnection);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_io_fedlet_mobutil_PushServiceImpl_onUnregisteredNative(
    JNIEnv* env, jobject /*thiz*/, jstring jInstance)
{
    QString instance = jstringToQString(env, jInstance);
    qDebug() << "[PushHandler] unregistered, instance:" << instance;
}

void PushHandler::startRegistrationTimeout()
{
    if (m_regTimeoutTimerId) {
        killTimer(m_regTimeoutTimerId);
    }
    m_regTimeoutTimerId = startTimer(10000);
}

void PushHandler::cancelRegistrationTimeout()
{
    if (m_regTimeoutTimerId) {
        killTimer(m_regTimeoutTimerId);
        m_regTimeoutTimerId = 0;
    }
}

void PushHandler::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_regTimeoutTimerId) {
        m_regTimeoutTimerId = 0;
        qWarning() << "[PushHandler] registration timeout - distributor not responding";
        emit registrationFailed(QString::fromUtf8("推送注册超时，请检查 ntfy 是否在运行, 安装: %1").arg(ntfyshPushInstalled));
    }
}

#else

void PushHandler::start() {}
void PushHandler::stop() {}
PushHandler* PushHandler::instance() { return nullptr; }
void PushHandler::registerDevice() {}
void PushHandler::selectDistributor(const QString&) {}
void PushHandler::startRegistrationTimeout() {}
void PushHandler::cancelRegistrationTimeout() {}
void PushHandler::timerEvent(QTimerEvent*) {}

#endif
