include ($$PWD/../appExport.pri)
include(../shared/qtsingleapplication/qtsingleapplication.pri)

TEMPLATE = app
CONFIG += qtc_runnable sliced_bundle
TARGET = $$IDE_APP_TARGET
DESTDIR = $$IDE_APP_PATH
VERSION = $$QTCREATOR_VERSION

QT  +=core gui widgets qml quick testlib sql network


SOURCES += main.cpp
