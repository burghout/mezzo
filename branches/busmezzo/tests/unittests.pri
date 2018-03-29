# Global  rules

include($$PWD/mezzo_lib.pri)

# This file has all the common elements for the unittests

QT       += testlib
QT       -= gui

CONFIG   += console
CONFIG   -= app_bundle #for mac to avoid making app bundles
CONFIG   += testcase

TEMPLATE = app

DEFINES += ﻿_NO_GUI
INCLUDEPATH += $$MEZZO_ROOT_RPATH
DEPENDPATH += $$MEZZO_ROOT_RPATH

#LIBS += -l


HEADERS += 

RESOURCES += \
    $$PWD/testdata.qrc # put testdata in here
LIBS += 

MOC_DIR =
OBJECTS_DIR =


DESTDIR = ../bin
