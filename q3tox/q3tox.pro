TEMPLATE = app
TARGET = q3tox
QT = core gui widgets
CONFIG += moc

SOURCES = main.cpp mainwindow.cpp restapi.cpp eventpoller.cpp \
             chatwidget.cpp chatview.cpp contactlist.cpp selfinfo.cpp translator.cpp \
             cJSON.c editinfodialog.cpp conferenceinvitedialog.cpp groupinvitedialog.cpp ThemeManager.cpp \
             friendinfodialog.cpp placeholderlineedit.cpp apilog.cpp memberlistdialog.cpp \
             appsetup.cpp compat34.cpp emojiutil.cpp emojiwidgets.cpp emojiitems.cpp messageinput.cpp \
             emoji_picker.cpp

HEADERS = mainwindow.h restapi.h eventpoller.h \
            chatwidget.h chatview.h contactlist.h selfinfo.h translator.h \
            editinfodialog.h conferenceinvitedialog.h groupinvitedialog.h ThemeManager.h compat34.h \
            friendinfodialog.h placeholderlineedit.h apilog.h memberlistdialog.h \
            appsetup.h appsetup_c.h emojiutil.h emojiwidgets.h emojiitems.h messageinput.h \
            emoji_picker.h

# LimeStyle theme system
LIME_STYLE_H = StyleParams.h LimeStyle.h LimeScrollBar.h
LIME_STYLE_CPP = LimeStyle.cpp LimeScrollBar.cpp

HEADERS += $$LIME_STYLE_H
SOURCES += $$LIME_STYLE_CPP

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
    message("Building for Qt4 - QT3_BUILD not defined")
} else {
    # Qt3: QT_VERSION 为空
    message("Building for Qt3 - adding QT3_BUILD")
    DEFINES += QT3_BUILD
}

# 包含路径
# INCLUDEPATH += /opt/qt338sh/include /opt/qt338sh/include/qt3

# FreeType2 自动检测
FREETYPE_LIBS = $$system(pkg-config --libs freetype2 2>/dev/null)
!isEmpty(FREETYPE_LIBS) {
    QMAKE_CXXFLAGS += $$system(pkg-config --cflags freetype2 2>/dev/null)
    LIBS += -lcurl $$FREETYPE_LIBS
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
    # override Makefile prefixed with $(QTDIR)
    INCLUDEPATH += $$(QTDIR)/include
    QMAKE_INCDIR_QT    = $$(QTDIR)/include
    QMAKE_LIBDIR_QT    = $$(QTDIR)/lib
    QMAKE_MOC          = $$(QTDIR)/bin/moc
    QMAKE_UIC          = $$(QTDIR)/bin/uic
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
