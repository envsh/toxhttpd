TEMPLATE = app
TARGET = qltox
QT = core gui widgets
CONFIG += moc
CONFIG += sdk_no_version_check
QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.7

SOURCES = main.cpp mainwindow.cpp restapi.cpp eventpoller.cpp \
             chatwidget.cpp chatview.cpp contactlist.cpp selfinfo.cpp translator.cpp \
             cJSON.c editinfodialog.cpp conferenceinvitedialog.cpp groupinvitedialog.cpp \
             friendinfodialog.cpp memberlistdialog.cpp logindialog.cpp \
             messageinput.cpp sound.c

HEADERS = mainwindow.h restapi.h eventpoller.h \
            chatwidget.h chatview.h contactlist.h selfinfo.h translator.h \
            editinfodialog.h conferenceinvitedialog.h groupinvitedialog.h \
            friendinfodialog.h memberlistdialog.h logindialog.h \
            messageinput.h sound.h
            
# LimeStyle theme system
LIME_STYLE_H = StyleParams.h LimeStyle.h LimeScrollBar.h
LIME_STYLE_CPP = StyleParams.cpp LimeStyle.cpp LimeScrollBar.cpp

QTCOMP_CPP = apilog.cpp appsetup.cpp compat34.cpp emojiutil.cpp emojiwidgets.cpp emojiitems.cpp \
			emoji_picker.cpp ThemeManager.cpp placeholderlineedit.cpp toastwidget.cpp
QTCOMP_HDR = apilog.h appsetup.h appsetup_c.h compat34.h emojiutil.h emojiwidgets.h emojiitems.h \
			emoji_picker.h ThemeManager.h placeholderlineedit.h toastwidget.h

HEADERS += $$LIME_STYLE_H $$QTCOMP_HDR
SOURCES += $$LIME_STYLE_CPP $$QTCOMP_CPP

# moc 处理
MOC_DIR = .
OBJECTS_DIR = .

# C++11 标准
QMAKE_CXXFLAGS += -std=c++11 -O0

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
    LIBS += -lcurl -lopenal $$FREETYPE_LIBS
    message("FreeType2: detected via pkg-config")
} else {
    INCLUDEPATH += /usr/include/freetype2
    LIBS += -lcurl -lfreetype
    message("FreeType2: pkg-config not found, using fallback paths")
}

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
