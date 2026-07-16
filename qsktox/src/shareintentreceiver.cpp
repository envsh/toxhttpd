#include "mainpage.h"
#include <QGuiApplication>
#include <QDebug>
#include <atomic>

static std::atomic<MainPage*> s_mainPage{nullptr};

void registerMainPage(MainPage* page)
{
    s_mainPage.store(page, std::memory_order_release);
}

#ifdef Q_OS_ANDROID
#include <jni.h>

extern "C" JNIEXPORT void JNICALL
Java_io_fedlet_mobutil_ShareActivity_onShareIntentReceived(
    JNIEnv* env, jobject /*thiz*/,
    jstring jAction, jstring jMimeType,
    jstring jText, jstring jUris)
{
    auto toStr = [env](jstring js) -> QString {
        if (!js) return {};
        const char* raw = env->GetStringUTFChars(js, nullptr);
        QString s = QString::fromUtf8(raw);
        env->ReleaseStringUTFChars(js, raw);
        return s;
    };

    QString action   = toStr(jAction);
    QString mimeType = toStr(jMimeType);
    QString text     = toStr(jText);
    QString urisJson = toStr(jUris);

    qDebug() << "[shareintentreceiver] action:" << action << "mime:" << mimeType;

    auto* page = s_mainPage.load(std::memory_order_acquire);
    if (page) {
        page->handleShareIntent(action, mimeType, text, urisJson);
    } else {
        qWarning() << "[shareintentreceiver] MainPage not found";
    }
}

#endif
