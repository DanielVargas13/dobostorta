TEMPLATE = app
TARGET = torta-dl
INCLUDEPATH += .
INSTALLS += target
target.path = /usr/local/bin

DEFINES += GIT_VERSION=\\\"$$system(git describe --always --tags --dirty)\\\"

QT += widgets network

SOURCES += main.cpp
