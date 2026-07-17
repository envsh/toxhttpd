#ifndef PLUGIN_LOADER_H
#define PLUGIN_LOADER_H

#include "compat34.h"
#include <qstring.h>

#define PLUGIN_TYPE_UIAPP 0
#define PLUGIN_TYPE_NOUI  1

struct PluginInfo {
    QString name;
    QString version;
    QString description;
    QString soPath;
    int     type;
    void*   handle;
    bool    enabled;

    void*  (*create)(void* parent);
    void   (*destroy)(void* widget);
    QWidget* widget;

    int    (*nouiInit)(void);
    void   (*nouiUninit)(void);
};

class PluginLoader {
public:
    static void scanPlugins();
    static void rescan();

    static int  uiappCount();
    static int  nouiCount();
    static const PluginInfo& uiappAt(int i);
    static const PluginInfo& nouiAt(int i);

    static QWidget* createUiApp(int index, QWidget* parent);
    static void     destroyUiApp(int index);
    static void     closeAllUiApps();

    static void initNouiPlugins();
    static void uninitNouiPlugins();

    static void setEnabled(int index, bool on);
    static bool isEnabled(int index);

    static void loadState();
    static void saveState();
};

QStringList pluginBaseDirs();

#endif
