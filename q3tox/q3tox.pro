TEMPLATE = app
TARGET = q3tox
QT = core gui
CONFIG += moc

SOURCES = main.cpp mainwindow.cpp api.cpp eventpoller.cpp \
           chatwidget.cpp contactlist.cpp selfinfo.cpp translator.cpp \
           cJSON.c editinfodialog.cpp invitedialog.cpp ThemeManager.cpp

HEADERS = mainwindow.h api.h eventpoller.h \
           chatwidget.h contactlist.h selfinfo.h translator.h \
           editinfodialog.h invitedialog.h ThemeManager.h

# moc 处理
MOC_DIR = .
OBJECTS_DIR = .

# C++11 标准
QMAKE_CXXFLAGS += -std=c++11 -O0 -DQT3_BUILD

# 包含路径
# INCLUDEPATH += /opt/qt338sh/include /opt/qt338sh/include/qt3

# 库路径和链接
# QMAKE_LIBDIR_FLAGS += -L/opt/qt338sh/lib
LIBS += -lqt-mt -lcurl

# 安装
target.path = /usr/local/bin
INSTALLS += target

# 翻译文件
translation.path = lang
translation.files = lang/*.json
INSTALLS += translation
