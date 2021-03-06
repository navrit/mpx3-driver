TEMPLATE = app
TARGET = TestMpx3Driver

QT -= gui
QT += network concurrent

CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/../build-debug
    OBJECTS_DIR = $$PWD/../build-debug/objects
    MOC_DIR     = $$PWD/../build-debug/moc
    UI_DIR      = $$PWD/../build-debug/ui
}

CONFIG(release, debug|release) {
    CONFIG *= console

    DESTDIR = $$PWD/../build
    OBJECTS_DIR = $$PWD/../build/objects
    MOC_DIR     = $$PWD/../build/moc
    UI_DIR      = $$PWD/../build/ui

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

message("Explicitly enabling AVX2 instructions. WARNING: Turn this off if you are using a processor that doesn't support AVX2!")
QMAKE_CFLAGS          *= -mavx2
QMAKE_CXXFLAGS        *= -mavx2

message("I will stop compliation on the first error")
QMAKE_CFLAGS          *= -Wfatal-errors
QMAKE_CXXFLAGS        *= -Wfatal-errors

message("Passing high optimisation flags to the linker")
QMAKE_LFLAGS_RELEASE  -= -Wl,-O1
QMAKE_LFLAGS_DEBUG    -= -Wl,-O1
QMAKE_LFLAGS          *= -Wl,-O3

equals(QMAKE_CXX, clang++) {
    message("Enabling C++17 support in clang++")
    CONFIG *= c++1z
}
equals(QMAKE_CXX, g++) {
    message("Enabling C++17 support in g++")
    CONFIG *= c++1z
}



INCLUDEPATH += libs

SOURCES += \
    UdpReceiver.cpp \
    SpidrController.cpp \
    SpidrDaq.cpp \
    ReceiverThread.cpp \
    ReceiverThreadC.cpp \
    FramebuilderThread.cpp \
    FramebuilderThreadC.cpp \
    FrameAssembler.cpp \
    ChipFrame.cpp \
    FrameSet.cpp \
    FrameSetManager.cpp \
    main.cpp

HEADERS += \
    UdpReceiver.h \
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
    packetcontainer.h \
    OMR.h \
    ChipFrame.h \
    FrameSet.h \
    FrameSetManager.h

CONFIG += static
