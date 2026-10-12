QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= link_prl
TEMPLATE = app
TARGET = generate_webp_fixture
SOURCES += ../tests/generate_webp_fixture.cpp
macx: QMAKE_LIBS_OPENGL = -framework OpenGL
