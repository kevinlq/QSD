include($$PWD/../../pluginExport.pri)

DEFINES += CORE_LIBRARY

TEMPLATE = lib

#TARGET = Core$${FILE_POSTFIX}

CONFIG     += plugin plugin_with_soname

QT += \
    network \
    printsupport \
    qml \
    sql

INCLUDEPATH +=$$PWD/../../libs

win32 {
    QT += gui-private # Uses QPlatformNativeInterface.
    LIBS += -lole32 -luser32
}

HEADERS += \
    $$PWD/Id.h \
    $$PWD/QCorePlugin.h \
    QSMainWindow.h

SOURCES += \
    $$PWD/Id.cpp \
    $$PWD/QCorePlugin.cpp \
    QSMainWindow.cpp

EXAMPLE_FILES = $$PWD/Core.json

DISTFILES +=\
    $$PWD/Core.json

LIBS +=-L$${DIR_DEPEND_DEST} -lUtils$${FILE_POSTFIX}
LIBS +=-L$${DIR_DEPEND_DEST} -lExtensionsystem$${FILE_POSTFIX}

