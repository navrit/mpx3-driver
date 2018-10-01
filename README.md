This Receiver UDP Thread binds to ports 8192-8195 on a specified address (ANY by default) and receive packets from the UDP streams from the SPIDR/emulator.

SpidrController and SpidrDaq are used to trigger readout as usual.

The idea is to just replace receiverThread, receiverThreadC, framebuilderThread and framebuilderThreadC and keep the interface to Dexter the exact same unless really necessary...

build --> cd src/; qmake && make -j12 && make clean && ./receiveUDPThread
