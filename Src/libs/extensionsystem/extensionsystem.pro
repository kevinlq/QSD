# 这个子项目是实现插件的关键，核心部分都在这里
# 2018年8月27日22:50:19

include ($$PWD/../../libsExport.pri)

QT += core gui widgets

TEMPLATE = lib

CONFIG += c++14

DEFINES += EXTENSIONSYSTEM_LIBRARY

unix:LIBS += $$QMAKE_LIBS_DYNLOAD

!isEmpty(vcproj) {
    DEFINES += IDE_TEST_DIR=\"$$IDE_SOURCE_TREE\"
} else {
    DEFINES += IDE_TEST_DIR=\\\"$$IDE_SOURCE_TREE\\\"
}


INCLUDEPATH +=$$PWD/../
INCLUDEPATH +=$$PWD/../aggregation

HEADERS += pluginerrorview.h \
    plugindetailsview.h \
    invoker.h \
    iplugin.h \
    iplugin_p.h \
    extensionsystem_global.h \
    pluginmanager.h \
    pluginmanager_p.h \
    pluginspec.h \
    pluginspec_p.h \
    pluginview.h \
    optionsparser.h \
    pluginerroroverview.h

SOURCES += pluginerrorview.cpp \
    plugindetailsview.cpp \
    invoker.cpp \
    iplugin.cpp \
    pluginmanager.cpp \
    pluginspec.cpp \
    pluginview.cpp \
    optionsparser.cpp \
    pluginerroroverview.cpp
FORMS += \
    pluginerrorview.ui \
    plugindetailsview.ui \
    pluginerroroverview.ui
