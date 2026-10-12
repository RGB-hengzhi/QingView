QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= link_prl
TEMPLATE = app
TARGET = format_probe
SOURCES += format_probe.cpp
macx: QMAKE_LIBS_OPENGL = -framework OpenGL
