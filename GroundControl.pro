#-------------------------------------------------
#
# Project created by QtCreator 2014-01-19T11:33:19
#
#-------------------------------------------------

QT      += serialport core gui network printsupport webkitwidgets
CONFIG  += console

greaterThan(QT_MAJOR_VERSION, 5): QT += widgets

TARGET   =  GroundControl
TEMPLATE =  app

SOURCES +=  main.cpp\
            mainwindow.cpp \
            qcustomplot.cpp \
            attitudeindicator.cpp \
            piddockwidget.cpp \
            piddialog.cpp\
            rcwidget.cpp \
            absframe.cpp \
            container.cpp\
            gmaps.cpp \
            trimprofile.cpp \
    compass.cpp

HEADERS  += mainwindow.h\
            qcustomplot.h \
            plotdockwidget.h \
            attitudeindicator.h \
            attitudedockwidget.h \
            rcwidget.h \
            logdockwidget.h \
            piddockwidget.h \
            piddialog.h \
            absframe.h \
            container.h \
            gmaps.h \
            trimprofile.h \
    compass.h

linux:LIBS += -lsfml-window \

win32:LIBS += -LC:/SFML-2.1/lib/ -lsfml-window-d
win32:LIBS += -LC:/SFML-2.2/lib/ -lsfml-window-d

win32:INCLUDEPATH += C:/SFML-2.1/include
win32:DEPENDPATH += C:/SFML-2.1/include

win32:INCLUDEPATH += C:/SFML-2.2/include
win32:DEPENDPATH += C:/SFML-2.2/include
