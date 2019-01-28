TEMPLATE = app

QT += qml quick widgets core

static {
    QTPLUGIN += qtvirtualkeyboardplugin
    QT += svg
}


DEFINES += CONFIG_CTRL_IFACE CONFIG_CTRL_IFACE_UNIX
SOURCES += wpa_supplicant/src/utils/os_unix.c

INCLUDEPATH +=$$PWD wpa_supplicant
include (wpa_supplicant/wpa_supplicant.pri)

SOURCES += main.cpp \
    model.cpp \
    apgui.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)

HEADERS += \
    model.h \
    apgui.h

