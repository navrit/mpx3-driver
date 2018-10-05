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
    uint64_t type = (*pixel_packet) & PKT_TYPE_MASK;
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
      [[fallthrough]];
    case INFO_HEADER_MID:
      //! This is really iMID (N*6) + iEOF (N*1) = 7*N
      ++iMID;
      [[fallthrough]];
    case INFO_HEADER_EOF:
      ++iEOF;
      //! TODO Come back to this logic, HOW DOES THIS EVER GET TRIGGERED?!
      if (type == INFO_HEADER_SOF) {
        infoIndex = 0;
      }
      if (infoIndex <= MPX_PIXEL_COLUMNS / 8 - 4) {
        for (int i = 0; i < 4; ++i, ++infoIndex) {
          infoHeader[infoIndex] =
              char((*pixel_packet >> (i * 8)) &
                   0xFF); // Same as infoHeader[infoIndex] = p[i] where p =
                          // (char *) pixel_packet;
        }
      }
      if (type == INFO_HEADER_EOF) {
        // Format and interpret:
        // e.g. OMR has to be mirrored per byte;
        // in order to distinguish CounterL from CounterH readout
        // it's sufficient to look at the last byte, containing
        // the three operation mode bits
        uint8_t byt = infoHeader[infoIndex - 1];
        // Mirroring of byte with bits 'abcdefgh':
        byt = ((byt & 0xF0) >> 4) | ((byt & 0x0F) << 4); // efghabcd
        byt = ((byt & 0xCC) >> 2) | ((byt & 0x33) << 2); // ghefcdab
        byt = ((byt & 0xAA) >> 1) | ((byt & 0x55) << 1); // hgfedcba
        if ((byt & 0x7) == 0x4) {                        // 'Read CounterH'
          isCounterhFrame = true;
        } else {
          isCounterhFrame = false;
        }
      }
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
