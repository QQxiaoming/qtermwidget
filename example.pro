
TARGET_ARCH=$${QT_ARCH}
CONFIG += c++11
DEFINES += QT_DEPRECATED_WARNINGS
QT += core gui widgets network xml multimedia

include(./lib/qtermwidget.pri)

SOURCES += \
    $$PWD/examples_main.cpp
    