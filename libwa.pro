TEMPLATE = lib

TARGET = wa-qt4
isEmpty(CURRENT_RPATH_DIR) {
    target.path = /usr/lib
} else {
    message("$$TARGET QMAKE_RPATHDIR and PATH is set to $$CURRENT_RPATH_DIR")
    target.path = $$CURRENT_RPATH_DIR
    QMAKE_RPATHDIR += $$INSTALL_ROOT$$CURRENT_RPATH_DIR
}
VERSION = 1.0.0

QT += network sql
CONFIG += dll

INSTALLS += target

DEFINES += LIBWA_LIBRARY

LIBS += -laxolotl
LIBS += -lcurve25519

HEADERS += \
    src/waregistration.h \
    src/libwa_global.h \
    src/libwa.h \
    src/waconnection.h \
    src/waconnection_p.h \
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
    src/watokendictionary.h \
    src/hmacsha1.h \
    src/json.h \
    src/axolotl/litesignedprekeystore.h \
    src/axolotl/litesessionstore.h \
    src/axolotl/liteprekeystore.h \
    src/axolotl/liteidentitykeystore.h \
    src/axolotl/liteaxolotlstore.h \
    src/mediadownloader.h

SOURCES += \
    src/waregistration.cpp \
    src/waconnection.cpp \
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
    src/watokendictionary.cpp \
    src/hmacsha1.cpp \
    src/json.cpp \
    src/axolotl/litesignedprekeystore.cpp \
    src/axolotl/litesessionstore.cpp \
    src/axolotl/liteprekeystore.cpp \
    src/axolotl/liteidentitykeystore.cpp \
    src/axolotl/liteaxolotlstore.cpp \
    src/mediadownloader.cpp

lessThan(QT_MAJOR_VERSION, 5) {
HEADERS += \
    src/qexception/qexception.h \
    src/qtjson.h
SOURCES +=  \
    src/qexception/qexception.cpp \
    src/qtjson.cpp
}
