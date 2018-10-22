#ifndef FRAMEASSEMBLER_H
#define FRAMEASSEMBLER_H

#include <stdint.h>

#include "OMR.h"
#include "FrameSetManager.h"
#include "UdpReceiver.h"
#include "PacketContainer.h"

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

class FrameAssembler {
public:
  FrameAssembler(int chipIndex);
  void setFrameSetManager (FrameSetManager * fsm) { this->fsm = fsm; }
  void onEvent(PacketContainer &pc);

  int infoIndex = 0;
  int chipId;
  OMR omr;

  int chipIndex;

  static void lutInit(bool lutBug);

private:
  FrameSetManager *fsm;
  int sizeofuint64_t = sizeof(uint64_t);
  int row_counter = -1, rubbish_counter = 0;
  uint16_t cursor = 55555;
  uint16_t counter_depth = 12;
  uint16_t counter_bits  = 12;
  uint16_t pixels_per_word =
      60 / counter_depth; //! TODO get or calculate pixel_depth
  uint32_t pixel_mask = 0xfff;
  uint16_t endCursor = 256;

  uint8_t frameId = 0;
  ChipFrame *frame = nullptr;
  uint16_t *row;

  inline uint8_t extractRow(uint64_t pixelword) { return uint8_t(((pixelword & ROW_COUNT_MASK) >> ROW_COUNT_SHIFT));}
  inline uint8_t extractFrameId(uint64_t pixelword) { return uint8_t(((pixelword & FRAME_FLAGS_MASK) >> FRAME_FLAGS_SHIFT)); }
  inline uint64_t packetType(uint64_t pixelword) { return (pixelword & PKT_TYPE_MASK); }
  inline bool packetEndsRow(uint64_t pixelword) { return (pixelword & 0x6000000000000000) == 0x6000000000000000; }

  uint64_t lutBugFix(uint64_t pixelword);

  // Look-up tables for Medipix3RX pixel data decoding
  static int   _mpx3Rx6BitsLut[64];
  static int   _mpx3Rx6BitsEnc[64];
  static int   _mpx3Rx12BitsLut[4096];
  static int   _mpx3Rx12BitsEnc[4096];
  static bool  _lutBug;

};
#endif // FRAMEASSEMBLER_H
