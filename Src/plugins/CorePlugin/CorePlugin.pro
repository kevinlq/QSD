include($$PWD/../../pluginExport.pri)

DEFINES += CORE_LIBRARY

TEMPLATE = lib

#TARGET = Core$${FILE_POSTFIX}

CONFIG     += plugin plugin_with_soname
CONFIG += C++11

QT += \
    network \
    printsupport \
    qml \
    sql

INCLUDEPATH +=$$PWD/../
INCLUDEPATH +=$$PWD/../../libs
INCLUDEPATH +=$$PWD/actionManager

win32 {
    QT += gui-private # Uses QPlatformNativeInterface.
    LIBS += -lole32 -luser32
}

HEADERS += \
    $$PWD/Id.h \
    $$PWD/QCorePlugin.h \
    $$PWD/QSMainWindow.h \
    QMenuManager.h

SOURCES += \
    $$PWD/Id.cpp \
    $$PWD/QCorePlugin.cpp \
    $$PWD/QSMainWindow.cpp \
    QMenuManager.cpp


EXAMPLE_FILES = $$PWD/Core.json

DISTFILES +=\
    $$PWD/Core.json

LIBS +=-L$${DIR_DEPEND_DEST} -lUtils$${FILE_POSTFIX}
LIBS +=-L$${DIR_DEPEND_DEST} -lExtensionsystem$${FILE_POSTFIX}

