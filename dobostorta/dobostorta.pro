TEMPLATE = app
TARGET = dobostorta
INCLUDEPATH += .
INSTALLS += target
target.path = /usr/local/bin

DEFINES += GIT_VERSION=\\\"$$system(git describe --always --tags)\\\"

QT += widgets webengine webenginewidgets sql

SOURCES += main.cpp
