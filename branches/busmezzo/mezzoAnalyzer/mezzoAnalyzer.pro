

TEMPLATE = lib
TARGET = mezzoAnalyzer
DEPENDPATH += . src ui

LIBS+= $(SUBLIBS) -L../mezzo_lib/Debug -lmezzo_lib 
#-L$(QTDIR)/lib -lQtCore -lQtGui 

QT+= core gui widgets

QMAKE= $(QTDIR)/bin/qmake
CONFIG +=  staticlib
#uic4

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x050F00
#QMAKE_LFLAGS +=

# Input
HEADERS += src/assist.h \
           src/odcheckerdlg.h \
           src/odtabledelegate.h 

FORMS += ui/odcheckdlg.ui
SOURCES += src/odcheckerdlg.cpp \
           src/odtabledelegate.cpp \

           
           
