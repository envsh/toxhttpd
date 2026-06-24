
# usage: include(/path/to/qlite.pri)

# todo self depends setting

# LimeStyle theme system
LIME_STYLE_H = $$PWD/StyleParams.h $$PWD/LimeStyle.h $$PWD/LimeScrollBar.h
LIME_STYLE_CPP = $$PWD/StyleParams.cpp $$PWD/LimeStyle.cpp $$PWD/LimeScrollBar.cpp

QTCOMP_CPP = $$PWD/limelog.cpp $$PWD/appsetup.cpp $$PWD/compat34.cpp $$PWD/translator.cpp \
			$$PWD/emojiutil.cpp $$PWD/emojiwidgets.cpp $$PWD/emojiitems.cpp \
			$$PWD/emoji_picker.cpp $$PWD/ThemeManager.cpp $$PWD/placeholderlineedit.cpp $$PWD/toastwidget.cpp \
			$$PWD/FramelessHelper.cpp $$PWD/CustomTitleBar.cpp $$PWD/ConfigDialog.cpp \
			$$PWD/sharedstatusbar.cpp \
			$$PWD/qthooks.cpp \
			$$PWD/jsonview.cpp \
			$$PWD/floatingpill.cpp \
			$$PWD/generic_slot.cpp

QTCOMP_HDR = $$PWD/limelog.h $$PWD/appsetup.h $$PWD/appsetup_c.h $$PWD/translator.h $$PWD/compat34.h \
			$$PWD/emojiutil.h $$PWD/emojiwidgets.h $$PWD/emojiitems.h \
			$$PWD/emoji_picker.h $$PWD/ThemeManager.h $$PWD/placeholderlineedit.h $$PWD/toastwidget.h \
			$$PWD/FramelessHelper.h $$PWD/CustomTitleBar.h $$PWD/EmbeddedMenuBar.h $$PWD/ConfigDialog.h \
			$$PWD/lambdaslot.h \
			$$PWD/sharedstatusbar.h \
			$$PWD/jsonview.h \
			$$PWD/floatingpill.h

HEADERS += $$LIME_STYLE_H $$QTCOMP_HDR
SOURCES += $$LIME_STYLE_CPP $$QTCOMP_CPP

