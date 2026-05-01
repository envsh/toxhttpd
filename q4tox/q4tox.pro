QT += core gui network
TARGET = q4tox
TEMPLATE = app

# Qt4 does not have QJsonDocument, use cJSON
SOURCES += main.cpp \
           api.cpp \
           eventpoller.cpp \
           translator.cpp \
           mainwindow.cpp \
           selfinfo.cpp \
           contactlist.cpp \
           chatwidget.cpp \
           editinfodialog.cpp \
           invitedialog.cpp \
           cJSON.c

HEADERS += api.h \
           eventpoller.h \
           translator.h \
           mainwindow.h \
           selfinfo.h \
           contactlist.h \
           chatwidget.h \
           editinfodialog.h \
           invitedialog.h \
           cJSON.h

# ThemeManager (optional, can be included in mainwindow)
SOURCES += ThemeManager.cpp
HEADERS += ThemeManager.h

# Install lang files
lang.files = lang/zh-CN.json lang/zh-TW.json lang/en-US.json
lang.path = /usr/local/share/q4tox/lang
INSTALLS += lang
