# QMAKE file for GUI project. Make changes in this file, NOT in the MAKEFILE

TEMPLATE = app
TARGET = mezzo_gui
UI_DIR= ui_h
INCLUDEPATH += . ../mezzoAnalyzer/ui_h $(QTDIR)/include ui_h

LIBS += -L.

macx {
    CONFIG(debug, debug|release) {
    #    INCLUDEPATH += ../mezzoAnalyzer/Debug
        LIBS +=  -L../../mezzo_lib/Debug -lmezzo_lib -L../../mezzoAnalyzer/Debug -lmezzoAnalyzer
        DEPENDS += ../../mezzo_lib/Debug/mezzo_lib.lib ../../mezzoAnalyzer/Debug/mezzoAnalyzer.lib
        INCLUDEPATH+= ../mezzoAnalyzer/Debug/ui_h
    } else {
        INCLUDEPATH += ../mezzoAnalyzer/Release/ui_h
        LIBS +=  -L../../mezzo_lib/Release -lmezzo_lib -L../../mezzoAnalyzer/Release -lmezzoAnalyzer
    }

            LIBS += -L$(QTDIR)/lib -framework QtCore -framework QtGui -framework QtNetwork


}

CONFIG(debug, debug|release) {
     LIBS +=  -L../mezzo_lib/Debug -lmezzo_lib -L../mezzoAnalyzer/Debug -lmezzoAnalyzer 
     DEPENDS += ../mezzo_lib/Debug/mezzo_lib.lib ../mezzoAnalyzer/Debug/mezzoAnalyzer.lib
 } else {
     LIBS +=  -L../mezzo_lib/Release -lmezzo_lib -L../mezzoAnalyzer/Release -lmezzoAnalyzer 
 }

#win32 { LIBS += -L$(QTDIR)/lib -lQtCore -lQtGui -lQtNetwork }


#-lQtDesigner 
#-lQt3Support

QT+= core gui widgets

CONFIG += uic4 embed_manifest_exe

# Input
HEADERS += canvas_qt4.h parametersdialog_qt4.h src/nodedlg.h src/batchrundlg.h src/outputview.h src/positionbackground.h src/find.h
FORMS += canvas_qt4.ui parametersdialog_qt4.ui ui/nodedlg.ui ui/batchrundlg.ui ui/outputview.ui ui/positionbackground.ui ui/find.ui
SOURCES += canvas_qt4.cpp main.cpp parametersdialog_qt4.cpp src/nodedlg.cpp src/batchrundlg.cpp src/outputview.cpp src/positionbackground.cpp src/find.cpp
RESOURCES += canvas_qt4.qrc 
RC_FILE = mezzo.rc
DEPENDPATH += . ./src ../mezzo_lib/src

