TEMPLATE = app
CONFIG += console c++11

LIBS += -L$$PWD/lib -lliberofs_lib_static
LIBS += -L$$PWD/lib -llibdeflate
LIBS += -L$$PWD/lib -llibzstd_static
LIBS += -L$$PWD/lib -lliblz4_static
LIBS += $$PWD/lib/libiconv.dll

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
