TEMPLATE = app
TARGET = TestMpx3Driver

QT -= gui
QT += network concurrent

CONFIG(release, debug|release) {
    message("Enabling all optimisation flags as qmake sees fit")
    CONFIG *= optimize_full
}

message("Using clang and clang++")
QMAKE_CC              = clang
QMAKE_LINK_C          = $$QMAKE_CC
QMAKE_LINK_C_SHLIB    = $$QMAKE_CC

QMAKE_CXX             = clang++
QMAKE_LINK            = $$QMAKE_CXX
QMAKE_LINK_SHLIB      = $$QMAKE_CXX

message("Explicitly enabling AVX2 instructions")
QMAKE_CFLAGS          *= -mavx2
QMAKE_CXXFLAGS        *= -mavx2 -Wfatal-errors

message("Passing high optimisation flags to the linker")
QMAKE_LFLAGS_RELEASE  -= -Wl,-O1
QMAKE_LFLAGS_DEBUG    -= -Wl,-O1
QMAKE_LFLAGS          *= -Wl,-O3

equals(QMAKE_CXX, clang++) {
    message("Enabling C++17 support in clang")
    CONFIG *= c++1z
}
equals(QMAKE_CXX, g++) {
    message("Enabling C++17 support in g++")
    CONFIG *= c++1z
}

SOURCES += TestDriver.cpp \
           receiveUDPThread.cpp \
           SpidrController.cpp \
           SpidrDaq.cpp \
           ReceiverThread.cpp \
           ReceiverThreadC.cpp \
           FramebuilderThread.cpp \
           FramebuilderThreadC.cpp \
           FrameAssembler.cpp

HEADERS += TestDriver.h \
           receiveUDPThread.h \
           SpidrController.h \
           SpidrDaq.h \
           mpx3defs.h \
           mpx3dacsdescr.h \
           spidrmpx3cmds.h \
           spidrdata.h \
           ReceiverThread.h \
           ReceiverThreadC.h \
           FramebuilderThread.h \
           FramebuilderThreadC.h \
           FrameAssembler.h \
           configs.h \
    packetcontainer.h

CONFIG += static
