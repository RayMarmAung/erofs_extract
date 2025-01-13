TEMPLATE = app
CONFIG += console c++11

LIBS += -L$$PWD/lib -lliberofs_lib_static
LIBS += -L$$PWD/lib -llibdeflate
LIBS += -L$$PWD/lib -llibzstd_static
LIBS += -L$$PWD/lib -lliblz4_static
LIBS += -L$$PWD/lib -lliblzma
LIBS += -L$$PWD/lib -lkernel32 -lws2_32 -lssp -liconv

SOURCES += \
        ErofsHardlinkHandle.cpp \
        ErofsNode.cpp \
        ExtractHelper.cpp \
        ExtractOperation.cpp \
        main.cpp

HEADERS += \
    CaseSensitiveInfo.h \
    ErofsHardlinkHandle.h \
    ErofsNode.h \
    ExtractHelper.h \
    ExtractOperation.h \
    ExtractState.h \
    Logging.h \
    Utils.h \
    threadpool.h
