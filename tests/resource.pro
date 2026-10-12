TEMPLATE = app
TARGET = qingview-resource-probe
VERSION = 0.1
QT += core gui widgets concurrent svg
CONFIG += c++14 console qv_disable_online_version_check
CONFIG -= app_bundle debug_and_release debug_and_release_target link_prl
DEFINES += VERSION=$$VERSION QV_DISABLE_ONLINE_VERSION_CHECK QT_NO_FOREACH

macx {
    LIBS += -framework Cocoa -framework CoreGraphics
    DEFINES += COCOA_LOADED
    QMAKE_CXXFLAGS += -include arm_acle.h
    QMAKE_LIBS_OPENGL = -framework OpenGL
}

INCLUDEPATH += $$PWD/../src
include($$PWD/../src/src.pri)
SOURCES -= $$absolute_path($$PWD/../src/main.cpp)
SOURCES += $$PWD/resource_probe.cpp
RESOURCES += $$PWD/../resources/resources.qrc

DESTDIR = bin
OBJECTS_DIR = build
MOC_DIR = build
UI_DIR = build
RCC_DIR = build
