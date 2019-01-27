include ($$PWD/../../libsExport.pri)


TEMPLATE = lib
DEFINES += AGGREGATION_LIBRARY

TARGET = Aggregate$${FILE_POSTFIX}

HEADERS = aggregate.h \
    aggregation_global.h

SOURCES = aggregate.cpp

