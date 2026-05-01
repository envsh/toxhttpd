TEMPLATE = app
TARGET = q3tox
QT = core gui
CONFIG += moc

SOURCES = main.cpp mainwindow.cpp api.cpp eventpoller.cpp \
           chatwidget.cpp contactlist.cpp selfinfo.cpp translator.cpp \
           cJSON.c editinfodialog.cpp invitedialog.cpp ThemeManager.cpp

HEADERS = mainwindow.h api.h eventpoller.h \
           chatwidget.h contactlist.h selfinfo.h translator.h \
           editinfodialog.h invitedialog.h ThemeManager.h compat34.h

# moc 处理
MOC_DIR = .
OBJECTS_DIR = .

# C++11 标准
QMAKE_CXXFLAGS += -std=c++11 -O0

# Qt 版本检测：Qt3 时定义 QT3_BUILD
# Qt3 qmake (1.07a) 不支持 contains(QT_VERSION)
# 通过检测 QT3_BUILD 环境变量来判断
QT3_BUILD_VAL = $$(QT3_BUILD)
!equals(QT3_BUILD_VAL, "1") {
    message("Building for Qt4 - QT3_BUILD not defined")
} else {
    message("Building for Qt3 - adding QT3_BUILD")
    DEFINES += QT3_BUILD
}

# 包含路径
# INCLUDEPATH += /opt/qt338sh/include /opt/qt338sh/include/qt3

# 库路径和链接
# QMAKE_LIBDIR_FLAGS += -L/opt/qt338sh/lib
LIBS += -lcurl
QT3_BUILD_VAL = $$(QT3_BUILD)
!equals(QT3_BUILD_VAL, "1") {
    # Qt4: Qt 库由 qmake 自动处理
    message("Linking with Qt4 libraries")
} else {
    # Qt3: 手动指定库
    LIBS += -lqt-mt
}

# 安装
target.path = /usr/local/bin
INSTALLS += target

# 翻译文件
translation.path = lang
translation.files = lang/*.json
INSTALLS += translation
