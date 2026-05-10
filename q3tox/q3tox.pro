TEMPLATE = app
TARGET = q3tox
QT = core gui widgets
CONFIG += moc

SOURCES = main.cpp mainwindow.cpp api.cpp eventpoller.cpp \
             chatwidget.cpp chatview.cpp contactlist.cpp selfinfo.cpp translator.cpp \
             cJSON.c editinfodialog.cpp conferenceinvitedialog.cpp groupinvitedialog.cpp ThemeManager.cpp \
             friendinfodialog.cpp placeholderlineedit.cpp apilog.cpp memberlistdialog.cpp \
             appsetup.cpp compat34.cpp

HEADERS = mainwindow.h api.h eventpoller.h \
            chatwidget.h chatview.h contactlist.h selfinfo.h translator.h \
            editinfodialog.h conferenceinvitedialog.h groupinvitedialog.h ThemeManager.h compat34.h \
            friendinfodialog.h placeholderlineedit.h apilog.h memberlistdialog.h \
            appsetup.h appsetup_c.h

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

# 库路径和链接
# QMAKE_LIBDIR_FLAGS += -L/opt/qt338sh/lib
LIBS += -lcurl
isEmpty(QT_VERSION) {
    # Qt3: 手动指定库
    LIBS += -lqt-mt
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
