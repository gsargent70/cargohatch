# Takes docs/screenshot.png; run it with bin/screenshot.
QT += core gui widgets concurrent dbus
CONFIG += c++17
TARGET = screenshot
TEMPLATE = app

INCLUDEPATH += ../../src
HEADERS += $$files(../../src/*.h)
SOURCES += screenshot.cpp $$files(../../src/*.cpp)
SOURCES -= ../../src/main.cpp
RESOURCES += ../../src/resources.qrc
