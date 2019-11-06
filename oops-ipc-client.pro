CONFIG -= qt

TEMPLATE = lib
DEFINES += OOPSIPCCLIENT_LIBRARY

CONFIG += c++11

SOURCES += \
    oops-ipc-client.cpp

HEADERS += \
    oops-ipc-client.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
