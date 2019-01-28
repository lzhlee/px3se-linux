#-------------------------------------------------
#
# Project created by QtCreator 2017-06-30T08:50:55
#
#-------------------------------------------------

QT       += core gui quickwidgets multimedia multimediawidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets



TARGET = cvbsView
TEMPLATE = app

CONFIG += link_pkgconfig
GST_VERSION = 1.0

PKGCONFIG += \
    gstreamer-$$GST_VERSION \
    gstreamer-base-$$GST_VERSION \
    gstreamer-audio-$$GST_VERSION \
    gstreamer-video-$$GST_VERSION \
    gstreamer-pbutils-$$GST_VERSION

DEFINES += HAVE_GST_PHOTOGRAPHY
LIBS += -lgstphotography-$$GST_VERSION
LIBS += -lcam_hal
LIBS += -ldrm
LIBS += -liep

DEFINES += GST_USE_UNSTABLE_API #prevents warnings because of unstable photography API

DEFINES += HAVE_GST_ENCODING_PROFILES

DEFINES += USE_V4L

INCLUDEPATH +=$$PWD base RkCifCamera
include(base/base.pri)
include(RkCifCamera/rkcamera.pri)

SOURCES += main.cpp\
        mainwindow.cpp \
    cameratopwidgets.cpp \
    camerawidgets.cpp \
    cameraquickcontentwidget.cpp \
    camerapreviewwidgets.cpp \
    global_value.cpp \
    rkCifCamera.cpp

HEADERS  += mainwindow.h \
    cameratopwidgets.h \
    camerawidgets.h \
    cameraquickcontentwidget.h \
    camerapreviewwidgets.h \
    global_value.h \
    rkCifCamera.h

RESOURCES += \
    res_main.qrc
