TEMPLATE = app
TARGET = qltox
QT = core gui widgets
CONFIG += moc
CONFIG += sdk_no_version_check
QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.7

SOURCES = main.cpp mainwindow.cpp storage.cpp restapi.cpp eventpoller.cpp \
             chatwidget.cpp chatview.cpp chatbuffer.cpp contactlist.cpp selfinfo.cpp \
             cJSON.c editinfodialog.cpp conferenceinvitedialog.cpp groupinvitedialog.cpp \
             friendinfodialog.cpp memberlistdialog.cpp logindialog.cpp \
             messageinput.cpp sound.c loadingbar.cpp \
              unknownparser.cpp photoviewer.cpp avatar_manager.cpp \
               media_shmem_cache.cpp \
               channel_db.cpp message_db.cpp \
                pending_db.cpp cache_db.cpp cache_fs.cpp \
                translate_util.cpp \
                config.cpp \
                sticker_db.cpp stickerpicker.cpp stickermanager.cpp

HEADERS = mainwindow.h storage.h restapi.h eventpoller.h \
             chatwidget.h chatview.h chatbuffer.h contactlist.h selfinfo.h \
             editinfodialog.h conferenceinvitedialog.h groupinvitedialog.h \
             friendinfodialog.h memberlistdialog.h logindialog.h \
              messageinput.h sound.h loadingbar.h \
               unknownparser.h photoviewer.h avatar_manager.h \
               media_shmem_cache.h \
               channel_db.h message_db.h \
                pending_db.h cache_db.h cache_fs.h \
                translate_util.h \
                config.h \
                sticker_db.h stickerpicker.h stickermanager.h
            
include(../qlcomp/qlite.pri)

# 使 qltox/ 中的 #include "compat34.h" 能找到 qlcomp/
INCLUDEPATH += ../qlcomp
macx {
    INCLUDEPATH += /opt/vcpkg/installed/x64-osx-dynamic/include
    LIBS += -L/opt/vcpkg/installed/x64-osx-dynamic/lib -Wl,-rpath,/opt/vcpkg/installed/x64-osx-dynamic/lib -lhjson
} else {
    INCLUDEPATH += /opt/vcpkg/installed/x64-linux-dynamic/include
    LIBS += -L/opt/vcpkg/installed/x64-linux-dynamic/lib -Wl,-rpath,/opt/vcpkg/installed/x64-linux-dynamic/lib -lhjson
}

# moc 处理
MOC_DIR = .
OBJECTS_DIR = .

# C++11 标准
QMAKE_CXXFLAGS += -std=c++11 -O0

!win32: QMAKE_LFLAGS += -rdynamic

# Qt 版本检测：必须用同一个 .pro 文件
# 方法：用 system() 调用 qmake -v，检测输出是否包含 "Qt 3" 或 "4."
# Qt3 qmake (1.07a) 不支持 QT_VERSION 变量
# Qt4 qmake 支持 QT_VERSION
# 检测逻辑：如果 QT_VERSION 不为空，则是 Qt4
!isEmpty(QT_VERSION) {
    # Qt4: QT_VERSION 不为空（如 "4.8.7"）
    message("Building for Qt4+ - QT3_BUILD not defined")
} else {
    # Qt3: QT_VERSION 为空
    message("Building for Qt3 - adding QT3_BUILD")
    DEFINES += QT3_BUILD
    # *nix/bsd/mac/wsl
    QMAKE_EXE = $$system(ps -p $PPID -o args= | head -1 | awk '{print $1}')
    QTDIR_AUTO = $$system(dirname $(dirname $$QMAKE_EXE))
    # message("qmake path: $$QMAKE_QMAKE , $$QMAKE_EXE, $$QTDIR_AUTO")
    isEmpty(QTDIR) {
		message("Auto QTDIR ... $$QTDIR_AUTO")
		# QMAKE_EXTRA_VARIABLES += QTDIR # not works
		QTDIR = $$QTDIR_AUTO # var not env
		# override Makefile prefixed with $(QTDIR)
		INCLUDEPATH += $$QTDIR/include
		QMAKE_INCDIR_QT    = $$QTDIR/include
		QMAKE_LIBDIR_QT    = $$QTDIR/lib
		QMAKE_MOC          = $$QTDIR/bin/moc
		QMAKE_UIC          = $$QTDIR/bin/uic
		QMAKE_QMAKE        = $$QMAKE_EXE
		QMAKE        = $$QMAKE_EXE # subdirs use this!!!
		QMAKE_LRELEASE     = $$QTDIR/bin/lrelease # no works
    }
}

# 包含路径
# INCLUDEPATH += /opt/qt338sh/include /opt/qt338sh/include/qt3

# PKG_CONFIG_PATH=/opt/vcpkg/installed/.../lib/pkgconfig qmake
QMAKE_CXXFLAGS += $$system(pkg-config --cflags openal 2>/dev/null)
INCLUDEPATH += $$system(pkg-config --cflags-only-I openal 2>/dev/null|sed 's/-I//g')
LIBS += $$system(pkg-config --libs openal 2>/dev/null)

# FreeType2 自动检测
FREETYPE_LIBS = $$system(pkg-config --libs freetype2 2>/dev/null)
!isEmpty(FREETYPE_LIBS) {
    QMAKE_CXXFLAGS += $$system(pkg-config --cflags freetype2 2>/dev/null)
    LIBS += -lcurl -lopenal -ldl $$FREETYPE_LIBS
    message("FreeType2: detected via pkg-config")
} else {
    INCLUDEPATH += /usr/include/freetype2
    LIBS += -lcurl -lfreetype
    message("FreeType2: pkg-config not found, using fallback paths")
}

# SQLite 依赖检测
SQLITE_CFLAGS = $$system(pkg-config --cflags sqlite3 2>/dev/null)
SQLITE_LIBS = $$system(pkg-config --libs sqlite3 2>/dev/null)
isEmpty(SQLITE_LIBS) {
    error("sqlite3 not found. Install libsqlite3-dev (Debian) or libsqlite3x-devel (RHEL)")
}
message("SQLite: $$SQLITE_LIBS")
INCLUDEPATH += $$SQLITE_CFLAGS
LIBS += $$SQLITE_LIBS
DEFINES += HAVE_SQLITE

# 库路径和链接
# QMAKE_LIBDIR_FLAGS += -L/opt/qt338sh/lib

# Emoji rendering with FreeType (color emoji fonts):
# Uncomment the next line to enable EMOJI_RENDER_QT34
DEFINES += EMOJI_RENDER_QT34
isEmpty(QT_VERSION) {
    # Qt3: 手动指定库
    # LIBS += $$(QMAKE_LIBS_QT_THREAD) -lqt-mt
} else {
    # Qt4: Qt 库由 qmake 自动处理
    message("Linking with Qt4 libraries")
}

# 安装
target.path = /usr/local/bin
INSTALLS += target

# 翻译文件
translation.path = lang
translation.files = lang/*.json
INSTALLS += translation

# 应用图标
QMAKE_ICON = app_icon.icns
RC_FILE = app_icon.rc
