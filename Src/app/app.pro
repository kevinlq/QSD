include ($$PWD/../appExport.pri)
include(../shared/qtsingleapplication/qtsingleapplication.pri)

TEMPLATE = app
CONFIG += qtc_runnable sliced_bundle

VERSION = $$QTCREATOR_VERSION

QT  +=core gui widgets qml quick testlib sql network

#DEFINES *=IDE_SETTINGSVARIANT

INCLUDEPATH +=$$PWD/../
INCLUDEPATH +=$$PWD/../libs

SOURCES += main.cpp

LIBS +=-L$${DIR_LIBEXEC_PATH} -lUtils$${FILE_POSTFIX}
LIBS +=-L$${DIR_LIBEXEC_PATH} -lExtensionsystem$${FILE_POSTFIX}
