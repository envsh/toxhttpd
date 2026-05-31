
# usage: include(/path/to/qlite.pri)

# todo self depends setting

# LimeStyle theme system
LIME_STYLE_H = StyleParams.h LimeStyle.h LimeScrollBar.h
LIME_STYLE_CPP = StyleParams.cpp LimeStyle.cpp LimeScrollBar.cpp

QTCOMP_CPP = apilog.cpp appsetup.cpp compat34.cpp translator.cpp \
			emojiutil.cpp emojiwidgets.cpp emojiitems.cpp \
			emoji_picker.cpp ThemeManager.cpp placeholderlineedit.cpp toastwidget.cpp \
			FramelessHelper.cpp CustomTitleBar.cpp ConfigDialog.cpp

QTCOMP_HDR = apilog.h appsetup.h appsetup_c.h translator.h compat34.h \
			emojiutil.h emojiwidgets.h emojiitems.h \
			emoji_picker.h ThemeManager.h placeholderlineedit.h toastwidget.h \
			FramelessHelper.h CustomTitleBar.h EmbeddedMenuBar.h ConfigDialog.h

HEADERS += $$LIME_STYLE_H $$QTCOMP_HDR
SOURCES += $$LIME_STYLE_CPP $$QTCOMP_CPP

