

TEMPLATE = lib
TARGET = mezzoAnalyzer
DEPENDPATH += . src ui
UI_DIR= ui_h
INCLUDEPATH += .. . ui_h $(QTDIR)/include $(QTDIR)/include/QtCore $(QTDIR)/include/QtGui $(QTDIR)/include/QtDesigner $(QTDIR)/include/QtNetwork $(QTDIR)/include/ActiveQt Debug/ui_h
#$(QTDIR)/include/Qt3Support 
LIBS+= $(SUBLIBS) -L../mezzo_lib/Debug -lmezzo_lib -L$(QTDIR)/lib -lQtCore -lQtGui 
# -lQt3Support 
#-lQtDesigner -lQtNetwork
QT+= core gui 
#qt3support 
# activeqt xml network svg
QMAKE= $(QTDIR)/bin/qmake
CONFIG += uic4 staticlib 
#debug
win32 {
     QMAKE_LFLAGS += /LARGEADDRESSAWARE
	 }

# Input
HEADERS += src/assist.h \
           src/odcheckerdlg.h \
           src/odtabledelegate.h 
#           src/odtablemodel.h
FORMS += ui/odcheckdlg.ui
SOURCES += src/odcheckerdlg.cpp \
           src/odtabledelegate.cpp \
#           src/odtablemodel.cpp
           
           
