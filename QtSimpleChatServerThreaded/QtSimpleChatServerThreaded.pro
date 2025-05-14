QT += core network

TARGET = chatserver
# minimal c++ version is c++11
CONFIG *= c++11
CONFIG *= warn_on
CONFIG *= release

TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

CONFIG(release, debug|release):BUILD_DIR = release
CONFIG(debug, debug|release):BUILD_DIR = debug

DESTDIR = $$BUILD_DIR
OBJECTS_DIR = $$BUILD_DIR/build
RCC_DIR = $$BUILD_DIR/build
MOC_DIR = $$BUILD_DIR/build


SOURCES += \
    server.cpp \
    servermain.cpp \
    chatserver.cpp \
    serverworker.cpp

HEADERS += \
    chatserver.h \
    chatserver.h \
    enums.h \
    server.h \
    serverworker.h

unix {
    message(Linux build)
    CONFIG *= link_pkgconfig qt
    INSTALL_ROOT = /usr/local
    EXEC_PATH = $${INSTALL_ROOT}/bin
}
