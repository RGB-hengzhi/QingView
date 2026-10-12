TEMPLATE = app
TARGET = generate_webp_fixture
QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle debug_and_release debug_and_release_target link_prl
SOURCES += $$PWD/generate_webp_fixture.cpp
macx {
    QMAKE_APPLE_DEVICE_ARCHS = arm64
    QMAKE_LIBS_OPENGL = -framework OpenGL
}
DESTDIR = bin
OBJECTS_DIR = build
MOC_DIR = build
