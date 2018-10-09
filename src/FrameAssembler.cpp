#include "FrameAssembler.h"
#include "receiveUDPThread.h"
#include <iomanip> // For pretty column printing --> std::setw()

FrameAssembler::FrameAssembler(int chipIndex) {
    this->chipIndex = chipIndex;
}

void FrameAssembler::onEvent(PacketContainer &pc) {
  if (pc.chipIndex != chipIndex) {
    return;
  }
  int packetSize = pc.size / sizeofuint64_t;
    if (row_counter >= 0) {
      // we're in a frame
      int eorIndex = (endCursor - cursor) / pixels_per_word;
      uint64_t endPacket = pc.data[eorIndex];
      uint64_t type = endPacket & PKT_TYPE_MASK;
      if (type == PIXEL_DATA_EOF || type == PIXEL_DATA_EOR) {
      // that's OK!
      } else {
      // bugger, we lost something, find first special packet
      int i = 0, nextRow;
      type = pc.data[i] & PKT_TYPE_MASK;
      switch (type)  {
        case PIXEL_DATA_SOR :
          // OK, that can happen on position 0, store the row, figure out the next one
          nextRow = extractRow(pc.data[endCursor/pixels_per_word]);
          cursor = 0;
          break;
        case PIXEL_DATA_SOF :
        case INFO_HEADER_SOF:
        case INFO_HEADER_MID:
        case INFO_HEADER_EOF:
          // somehow we found a new frame, store the current row/frame
          row_counter = 0;
          cursor = 0;
          break;
        case PIXEL_DATA_MID:
          while (i < packetSize && (pc.data[i] & PKT_TYPE_MASK) == PIXEL_DATA_MID) i++;
          [[fallthrough]];
        case PIXEL_DATA_EOR :
        case PIXEL_DATA_EOF :
          int nextRow = extractRow(pc.data[i]);
          if (nextRow < rownr_SOR) {
          // we lost the rest of the frame; finish the current and start a new one
          } else {
          // we lost part of this frame; store the row, start a new one
          }
          cursor = endCursor - i * pixels_per_word;
          row_counter = nextRow;
          for (int j = 0; j < cursor; j++) row[j] = 0;
          break;
      }
      }
    }

  //! Start processing the pixel packet
  uint64_t *pixel_packet = (uint64_t *)pc.data;
  //row_number_from_packet = -1;
  for (int j = 0; j < packetSize; ++j, ++pixel_packet) {
    uint64_t pixelword = *pixel_packet;
    uint64_t type = pixelword & PKT_TYPE_MASK;

    switch (type) {
    case PIXEL_DATA_SOF:
      ++pSOF;
      --pSOR;
      [[fallthrough]];
    case PIXEL_DATA_SOR:
      ++pSOR;
      ++row_counter;
      ++rownr_SOR;
      --pMID;
      [[fallthrough]];
    case PIXEL_DATA_MID:
      ++pMID;
      break;
    case PIXEL_DATA_EOF:
      ++pEOF;
      frameId = extractFrameId(pixelword);
      row_counter = -1;
      cursor = -1;
      --pEOR;
      [[fallthrough]];
    case PIXEL_DATA_EOR:
      ++pEOR;
      //row_number_from_packet = extractRow(pixelword);
      ++rownr_EOR;
      break;
    case INFO_HEADER_SOF:
      //! This is really iSOF (N*1) + iMID (N*6) + iEOF (N*1) = 8*N
      ++iSOF;
      infoIndex = 0; break;
    case INFO_HEADER_MID:
      //! This is really iMID (N*6) + iEOF (N*1) = 7*N
      ++iMID;
      if (infoIndex == 4)
        chipId = (int) (pixelword & 0xffffffff);
      else if (infoIndex == 5 && chipId != 0) {
        omr.setHighR(pixelword & 0xffff);
      }
      infoIndex++; break;
    case INFO_HEADER_EOF:
      ++iEOF;
      omr.setLowR(pixelword & 0xffffffff);
      switch (omr.getCountL()) {
        case 0: counter_depth = 1; break;
        case 1: counter_depth = 6; break;
        case 2: counter_depth = 12; break;
        case 3: counter_depth = 24; break;
      }
      counter_bits = counter_depth == 24 ? 12 : counter_depth;
      pixels_per_word = 60 / counter_bits;
      pixel_mask = (1 << counter_bits) - 1;
      isCounterhFrame = omr.getMode() == 1;
      endCursor = MPX_PIXEL_COLUMNS - (MPX_PIXEL_COLUMNS % pixels_per_word);
      break;
    default:
      // Rubbish packets - skip these
      // In theory, there should be none ever
      if (type != 0) {
        std::cout << "Rubbish packet count: " << rubbish_counter << ": " << type
                  << "\n";
        ++rubbish_counter;
      }
      ++def;
      break;
    }
  }
}
