#ifndef FRAMEASSEMBLER_H
#define FRAMEASSEMBLER_H
#include "receiveUDPThread.h"
#include <stdint.h>

//! See Table 54 (MPX3 Packet Format) - SPIDR Register Map
//! 64 bit masks because of the uint64_t data type
//! Same order as switch, case statement
const static uint64_t PKT_TYPE_MASK = 0xF000000000000000;

const static uint64_t PIXEL_DATA_SOR = 0xA000000000000000;  //! #2
const static uint64_t PIXEL_DATA_EOR = 0xE000000000000000;  //! #3
const static uint64_t PIXEL_DATA_SOF = 0xB000000000000000;  //! #7
const static uint64_t PIXEL_DATA_EOF = 0x7000000000000000;  //! #5
const static uint64_t PIXEL_DATA_MID = 0x3000000000000000;  //! #1 Most frequent
const static uint64_t INFO_HEADER_SOF = 0x9000000000000000; //! #6
const static uint64_t INFO_HEADER_MID = 0x1000000000000000; //! #4
const static uint64_t INFO_HEADER_EOF = 0xD000000000000000; //! #8
//! End of Table 54
const static uint64_t ROW_COUNT_MASK = 0x0FF0000000000000;
const static uint64_t ROW_COUNT_SHIFT = 52;
const static uint64_t FRAME_FLAGS_MASK = 0x000FFFF000000000;
const static uint64_t FRAME_FLAGS_SHIFT = 36;

const static int MPX_PIXEL_COLUMNS = 256;

class FrameAssembler {
public:
  FrameAssembler(int chipIndex);
  void onEvent(receiveUDPThread::PacketContainer &pc);

  int infoIndex = 0;
  char infoHeader[MPX_PIXEL_COLUMNS/8]; //! Single info header (Contains an OMR)
  bool isCounterhFrame = false;

  int chipIndex;
  uint64_t pSOR = 0, pEOR = 0, pSOF = 0, pEOF = 0, pMID = 0, iSOF = 0, iMID = 0,
           iEOF = 0, def = 0;

private:
  int rowPixels = 0, row_counter = -1, rubbish_counter = 0;
  int counter_depth = 12;
  int pixels_per_word =
      60 / counter_depth; //! TODO get or calculate pixel_depth

  void hexdumpAndParsePacket(uint64_t* pixel_packet, int counter_bits, bool skip_data_packets, int chip);
};
#endif // FRAMEASSEMBLER_H
