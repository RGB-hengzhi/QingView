TEMPLATE = app
TARGET = qingview-integration
VERSION = 0.1
QT += core gui widgets concurrent svg testlib
CONFIG += c++14 console testcase qv_disable_online_version_check
CONFIG -= app_bundle debug_and_release debug_and_release_target link_prl
DEFINES += VERSION=$$VERSION QV_DISABLE_ONLINE_VERSION_CHECK QT_NO_FOREACH

macx {
    LIBS += -framework Cocoa
    DEFINES += COCOA_LOADED
    # Xcode's macOS 27 SDK requires this before CoreFoundation's vector headers.
    QMAKE_CXXFLAGS += -include arm_acle.h
    QMAKE_LIBS_OPENGL = -framework OpenGL
}

INCLUDEPATH += $$PWD/../src
include($$PWD/../src/src.pri)
SOURCES -= $$absolute_path($$PWD/../src/main.cpp)
SOURCES += $$PWD/tst_integration.cpp
RESOURCES += $$PWD/../resources/resources.qrc

DESTDIR = bin
OBJECTS_DIR = build
MOC_DIR = build
UI_DIR = build
RCC_DIR = build
