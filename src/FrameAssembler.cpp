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
  //! Start processing the pixel packet
  uint64_t *pixel_packet = (uint64_t *)pc.data;
  // row_number_from_packet = -1;
  int rownr_EOR = -1, rownr_SOR = -1;
  int sizeofuint64_t = sizeof(uint64_t);
  for (int j = 0; j < pc.size / sizeofuint64_t; ++j, ++pixel_packet) {
    uint64_t pixelword = *pixel_packet;
    uint64_t type = pixelword & PKT_TYPE_MASK;
    /*if (i == 0) {
        hexdumpAndParsePacket(pixel_packet, counter_depth, true, i);
    }*/
    switch (type) {
    case PIXEL_DATA_SOR:
      ++pSOR;
      // Henk checks for lost packets here
      // Henk checks for row counter > 256, when would this ever happen?
      ++row_counter;
      rowPixels = 0;
      rowPixels += pixels_per_word;
      ++rownr_SOR;
      break;
    case PIXEL_DATA_EOR:
      ++pEOR;
      // Henk checks for lost pixels again
      // Henk checks for row counter > 256, when would this ever happen?
      rowPixels += MPX_PIXEL_COLUMNS -
                   (MPX_PIXEL_COLUMNS / pixels_per_word) * pixels_per_word;
      ++rownr_EOR;
      /*if (rownr_SOR+1 != rownr_EOR) {
          std::cout << "Row # SOR: " << rownr_SOR << " " << pSOF << " - Row #
      EOR: " << rownr_EOR << " " << pEOF << "\n";
      }*/
      break;
    case PIXEL_DATA_SOF:
      ++pSOF;
      // Henk checks for lost pixels again
      rowPixels = 0;
      rowPixels += pixels_per_word;
      ++row_counter;
      break;
    case PIXEL_DATA_EOF:
      ++pEOF;
      // Henk checks for lost pixels again
      rowPixels += pixels_per_word;
      //! Henk extracts the FLAGS word here.
      //! Dexter doesn't use this yet, maybe revisit later
      /*row_number_from_packet = int(((*pixel_packet & ROW_COUNT_MASK) >>
      ROW_COUNT_SHIFT)); if (row_number_from_packet != 255) { std::cout << ">> "
      << row_number_from_packet << "\n";
      }*/
      break;
    case PIXEL_DATA_MID:
      ++pMID;
      rowPixels += pixels_per_word;
      if (rowPixels > MPX_PIXEL_COLUMNS) {
        ++row_counter;
      }
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
