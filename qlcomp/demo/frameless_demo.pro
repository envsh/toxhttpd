TEMPLATE = app
TARGET = frameless_demo
CONFIG += console
CONFIG -= app_bundle

SOURCES = frameless_demo.cpp \
    ../FramelessHelper.cpp

HEADERS = ../FramelessHelper.h

INCLUDEPATH += ..

MOC_DIR = .
OBJECTS_DIR = .
QMAKE_CXXFLAGS += -std=c++11 -O0 -w

!isEmpty(QT_VERSION) {
    QT = core gui widgets
} else {
    DEFINES += QT3_BUILD
    INCLUDEPATH += $$(QTDIR)/include
    QMAKE_INCDIR_QT = $$(QTDIR)/include
    QMAKE_LIBDIR_QT = $$(QTDIR)/lib
}
