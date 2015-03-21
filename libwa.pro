TEMPLATE = lib

TARGET = wa-qt5
target.path = /usr/share/harbour-mitakuuluu3/lib

QT += network
CONFIG += dll

INSTALLS += target

DEFINES += LIBWA_LIBRARY

HEADERS += \
    src/waregistration.h \
    src/libwa_global.h \
    src/libwa.h \
    src/waconnection.h \
    src/waconnection_p.h \
    src/funrequest.h \
    src/warequest.h \
    src/waconstants.h \
    src/attributelist.h \
    src/attributelistiterator.h \
    src/bintreenodereader.h \
    src/bintreenodewriter.h \
    src/key.h \
    src/keystream.h \
    src/protocoltreenode.h \
    src/protocoltreenodelist.h \
    src/protocoltreenodelistiterator.h \
    src/rc4.h \
    src/qtrfc2898.h \
    src/protocolexception.h \
    src/waexception.h \
    src/watokendictionary.h

SOURCES += \
    src/waregistration.cpp \
    src/waconnection.cpp \
    src/funrequest.cpp \
    src/warequest.cpp \
    src/attributelist.cpp \
    src/attributelistiterator.cpp \
    src/bintreenodereader.cpp \
    src/bintreenodewriter.cpp \
    src/key.cpp \
    src/keystream.cpp \
    src/protocoltreenode.cpp \
    src/protocoltreenodelist.cpp \
    src/protocoltreenodelistiterator.cpp \
    src/rc4.cpp \
    src/qtrfc2898.cpp \
    src/watokendictionary.cpp
