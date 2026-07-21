# mdeditor.pri — Markdown editor module
# Usage: include(../mdeditor/mdeditor.pri)

MDEDITOR_CPP = $$PWD/mdeditor.cpp $$PWD/mdhighlighter.cpp \
               $$PWD/mdpreview.cpp $$PWD/mdtoolbar.cpp \
               $$PWD/vendor/md4c.c $$PWD/vendor/md4c-html.c \
               $$PWD/vendor/entity.c
MDEDITOR_HDR = $$PWD/mdeditor.h $$PWD/mdhighlighter.h \
               $$PWD/mdpreview.h $$PWD/mdtoolbar.h \
               $$PWD/vendor/md4c.h $$PWD/vendor/md4c-html.h \
               $$PWD/vendor/entity.h

HEADERS += $$MDEDITOR_HDR
SOURCES += $$MDEDITOR_CPP
INCLUDEPATH += $$PWD $$PWD/vendor
