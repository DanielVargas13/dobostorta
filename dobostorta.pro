TEMPLATE = app
TARGET = dobostorta
INCLUDEPATH += .
INSTALLS += target
target.path = /usr/local/bin

QT += widgets webengine webenginewidgets sql

SOURCES += main.cpp
