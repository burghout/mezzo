# QMAKE file for GUI project. Make changes in this file, NOT in the MAKEFILE

TEMPLATE = app
TARGET = mezzo_gui
#DEFINES += _BUSES
INCLUDEPATH += ../mezzoAnalyzer/Debug ../mezzoAnalyzer $(QTDIR)/include ui_h
#INCLUDEPATH += . $(QTDIR)/include $(QTDIR)/include/QtCore $(QTDIR)/include/QtGui $(QTDIR)/include/QtDesigner $(QTDIR)/include/QtNetwork $(QTDIR)/include/ActiveQt

LIBS += -L.

macx {
    CONFIG(debug, debug|release) {
    #    INCLUDEPATH += ../mezzoAnalyzer/Debug
        LIBS +=  -L../../mezzo_lib/Debug -lmezzo_lib -L../../mezzoAnalyzer/Debug -lmezzoAnalyzer
        DEPENDS += ../../mezzo_lib/Debug/mezzo_lib.lib ../../mezzoAnalyzer/Debug/mezzoAnalyzer.lib
        INCLUDEPATH+= ../mezzoAnalyzer/Debug/
    } else {
        INCLUDEPATH += ../mezzoAnalyzer/Release/ui_h
        LIBS +=  -L../../mezzo_lib/Release -lmezzo_lib -L../../mezzoAnalyzer/Release -lmezzoAnalyzer
    }

            LIBS += -L$(QTDIR)/lib -framework QtCore -framework QtGui -framework QtNetwork
}


win32{
    CONFIG(debug, debug|release) {
         LIBS +=  -L../mezzo_lib/Debug -lmezzo_lib -L../mezzoAnalyzer/Debug -lmezzoAnalyzer
        DEPENDS += ../mezzo_lib/Debug/mezzo_lib.lib ../mezzoAnalyzer/Debug/mezzoAnalyzer.lib
		#LIBS +=$(SUBLIBS) -L$(QTDIR)/lib -lQt5Cored -lQt5Guid -lQt5Networkd  -lQt5Designerd
    } else {
         LIBS +=  -L../mezzo_lib/Release -lmezzo_lib -L../mezzoAnalyzer/Release -lmezzoAnalyzer
		 #LIBS +=$(SUBLIBS) -L$(QTDIR)/lib -lQt5Core -lQt5Gui -lQt5Network  -lQt5Designer
    }
}


QT+= core gui widgets
 
#activeqt xml network svg
CONFIG += uic4 embed_manifest_exe


# Input
HEADERS += canvas_qt4.h parametersdialog_qt4.h src/nodedlg.h src/batchrundlg.h src/outputview.h src/positionbackground.h
FORMS += canvas_qt4.ui parametersdialog_qt4.ui ui/nodedlg.ui ui/batchrundlg.ui ui/outputview.ui ui/positionbackground.ui
SOURCES += canvas_qt4.cpp main.cpp parametersdialog_qt4.cpp src/nodedlg.cpp src/batchrundlg.cpp src/outputview.cpp src/positionbackground.cpp
RESOURCES += canvas_qt4.qrc 
RC_FILE = mezzo.rc
DEPENDPATH += . ./src ../mezzo_lib/src
QMAKE_LFLAGS +=
