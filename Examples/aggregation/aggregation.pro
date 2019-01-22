include ($$PWD/../examples.pri)
include ($$PWD/../examples_Sup.pri)

TARGET = aggregation

TEMPLATE = app

QT += core gui widgets

DEFINES += AGGREGATION_LIBRARY
INCLUDEPATH += $${aggregationPath}/

SOURCES += main.cpp \
    $${aggregationPath}/aggregate.cpp

HEADERS += main.h \
    myinterfaces.h \
    $${aggregationPath}/aggregate.h \
    $${aggregationPath}/aggregation_global.h

FORMS += main.ui

