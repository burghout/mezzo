# Makes standalone version of Mezzo
TEMPLATE = app
CONFIG += console
QT -= gui
TARGET = mezzo_s
#DEFINES += _BUSES
DEFINES += _NO_GUI
macx {
    CONFIG   -= app_bundle # for mac remove the application bundling
}
#win32 {
 #    QMAKE_LFLAGS +=
#}

# Input
HEADERS += ../mezzo_lib/src/busline.h \
		   ../mezzo_lib/src/day2day.h \
           ../mezzo_lib/src/eventlist.h \
           ../mezzo_lib/src/gettime.h \
           ../mezzo_lib/src/Graph.h \
           ../mezzo_lib/src/grid.h \
           ../mezzo_lib/src/icons.h \
           ../mezzo_lib/src/link.h \
           ../mezzo_lib/src/linktimes.h \
           ../mezzo_lib/src/MMath.h \
           ../mezzo_lib/src/network.h \
           ../mezzo_lib/src/node.h \
           ../mezzo_lib/src/od.h \
           ../mezzo_lib/src/parameters.h \
           ../mezzo_lib/src/pvm.h \
           ../mezzo_lib/src/q.h \
           ../mezzo_lib/src/Random.h \
           ../mezzo_lib/src/route.h \
           ../mezzo_lib/src/sdfunc.h \
           ../mezzo_lib/src/server.h \
           ../mezzo_lib/src/trafficsignal.h \
           ../mezzo_lib/src/signature.h \
           ../mezzo_lib/src/turning.h \
           ../mezzo_lib/src/vehicle.h \
           ../mezzo_lib/src/vissimcom.h \
           ../mezzo_lib/src/vtypes.h \
           ../mezzo_lib/src/passenger.h \
           ../mezzo_lib/src/od_stops.h \
           ../mezzo_lib/src/pass_route.h \
		   ../mezzo_lib/src/controlcenter.h
SOURCES += ../mezzo_lib/src/busline.cpp \
		   ../mezzo_lib/src/day2day.cpp \
           ../mezzo_lib/src/eventlist.cpp \
           ../mezzo_lib/src/Graph.cpp \
           ../mezzo_lib/src/grid.cpp \
           ../mezzo_lib/src/icons.cpp \
           ../mezzo_lib/src/link.cpp \
           ../mezzo_lib/src/linktimes.cpp \
           ../mezzo_lib/src/network.cpp \
           ../mezzo_lib/src/node.cpp \
           ../mezzo_lib/src/od.cpp \
           ../mezzo_lib/src/parameters.cpp \
           ../mezzo_lib/src/pvm.cpp \
           ../mezzo_lib/src/q.cpp \
           ../mezzo_lib/src/Random.cpp \
           ../mezzo_lib/src/route.cpp \
           ../mezzo_lib/src/sdfunc.cpp \
           ../mezzo_lib/src/server.cpp \
           ../mezzo_lib/src/trafficsignal.cpp \
           ../mezzo_lib/src/signature.cpp \
           ../mezzo_lib/src/turning.cpp \
           ../mezzo_lib/src/vehicle.cpp \
           ../mezzo_lib/src/vissimcom.cpp \
           ../mezzo_lib/src/vtypes.cpp \
           ../mezzo_lib/src/passenger.cpp \
           ../mezzo_lib/src/od_stops.cpp \
           ../mezzo_lib/src/pass_route.cpp \
		   ../mezzo_lib/src/controlcenter.cpp \
		    ../mezzo_lib/src/main.cpp
