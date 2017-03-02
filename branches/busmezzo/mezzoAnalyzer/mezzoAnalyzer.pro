

TEMPLATE = lib
TARGET = mezzoAnalyzer
DEPENDPATH += . src ui
#INCLUDEPATH += . $(QTDIR)/include $(QTDIR)/include/QtCore $(QTDIR)/include/QtGui $(QTDIR)/include/QtDesigner $(QTDIR)/include/QtNetwork $(QTDIR)/include/ActiveQt 

LIBS+= $(SUBLIBS) -L../mezzo_lib/Debug -lmezzo_lib 
#-L$(QTDIR)/lib -lQtCore -lQtGui 

QT+= core gui widgets

QMAKE= $(QTDIR)/bin/qmake
CONFIG += uic4 staticlib 
#debug
QMAKE_LFLAGS +=

# Input
HEADERS += src/assist.h \
           src/odcheckerdlg.h \
           src/odtabledelegate.h 

FORMS += ui/odcheckdlg.ui
SOURCES += src/odcheckerdlg.cpp \
           src/odtabledelegate.cpp \

           
           
