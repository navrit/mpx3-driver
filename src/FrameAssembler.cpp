#include "FrameAssembler.h"
#include "receiveUDPThread.h"
#include <iomanip> // For pretty column printing --> std::setw()

//#define SKIPMOSTPIXELS

FrameAssembler::FrameAssembler(int chipIndex) {
    this->chipIndex = chipIndex;
}

void FrameAssembler::onEvent(PacketContainer &pc) {

  if (pc.chipIndex != chipIndex) {
    return;
  }
  uint64_t *pixel_packet = reinterpret_cast<uint64_t *>(pc.data);
  uint64_t packetSize = uint64_t(pc.size / sizeofuint64_t);
  bool packetLoss;
    if (row_counter >= 0) {
      // we're in a frame
      int eorIndex = (endCursor - cursor) / pixels_per_word;
      packetLoss = ! packetEndsRow(pixel_packet[eorIndex]);
    } else {
      packetLoss = packetType(pixel_packet[0]) != INFO_HEADER_SOF;
    }
    if (packetLoss) {
      // bugger, we lost something, find first special packet
      uint16_t i = 0;
      switch (packetType(pixel_packet[i])) {
        case PIXEL_DATA_SOR :
          // OK, that can happen on position 0, store the row, claim we finished the previous
          row_counter = extractRow(pixel_packet[endCursor/pixels_per_word]) - 1;
          assert (row_counter >= 0 && row_counter < 256);
          break;
        case PIXEL_DATA_SOF :
        case INFO_HEADER_SOF:
        case INFO_HEADER_MID:
        case INFO_HEADER_EOF:
          // somehow we found a new frame, store the current row/frame
          fsm->putChipFrame(frameId, chipIndex, frame);
          frame = nullptr;
          break;
        case PIXEL_DATA_MID:
          while (i < packetSize && packetType(pixel_packet[i]) == PIXEL_DATA_MID) i++;
          [[fallthrough]];
        case PIXEL_DATA_EOR :
        case PIXEL_DATA_EOF :
          assert (i < packetSize);
          int nextRow = extractRow(pixel_packet[i]);
          assert (nextRow >= 0 && nextRow < 256);
          if (frame == nullptr || nextRow < row_counter) {
            // we lost the rest of the frame; finish the current and start a new one
            if (frame != nullptr)
                fsm->putChipFrame(frameId, chipIndex, frame);
            frame = fsm->newChipFrame();
            if (counter_depth == 24) {
                if (omr.getMode() == 1) {
                    // finished high, next frame
                    omr.setMode(0);
                    frameId++;
                } else {
                    // finished low, expect high
                    omr.setMode(1);
                }
            } else {
                frameId++;
            }
            frame->omr = omr;
          } else {
            // we lost part of this frame; store the row, start a new one
          }
          cursor = endCursor - i * pixels_per_word;
          assert (cursor >= 0 && cursor < 256);
          assert (packetEndsRow(pixel_packet[(endCursor - cursor) / pixels_per_word]));
          row_counter = nextRow;
          row = frame->getRow(row_counter);
          break;
      }
    }

  //! Start processing the pixel packet
  //row_number_from_packet = -1;
  for (unsigned j = 0; j < packetSize; ++j, ++pixel_packet) {
    uint64_t pixelword = *pixel_packet;
    uint64_t type = pixelword & PKT_TYPE_MASK;

    switch (type) {
    case PIXEL_DATA_SOF:
      ++pSOF;
      row_counter = -1;
      --pSOR;
      [[fallthrough]];
    case PIXEL_DATA_SOR:
      ++pSOR;
      ++row_counter;
      ++rownr_SOR;
      assert (row_counter >= 0 && row_counter < 256);
      row = frame->getRow(row_counter);
      cursor = 0;
      --pMID;
      [[fallthrough]];
    case PIXEL_DATA_MID:
      ++pMID;
#ifdef SKIPMOSTPIXELS
      cursor += pixels_per_word;
#else
      for(int k=0; k<pixels_per_word; ++k, ++cursor ) {
        row[cursor] = uint16_t(pixelword & pixel_mask);
        pixelword >>= counter_bits;
      }
#endif
      assert (cursor < 256);
      break;
    case PIXEL_DATA_EOF:
      ++pEOF;
      frameId = extractFrameId(pixelword);
      row_counter = -1;
      cursor = 55555;
      --pEOR;
      [[fallthrough]];
    case PIXEL_DATA_EOR:
      ++pEOR;
      for (; cursor < 256; cursor++) {
        row[cursor] = uint16_t(pixelword & pixel_mask);
        pixelword >>= counter_bits;
      }
      if (type == PIXEL_DATA_EOF) {
          // we're done with this one!
          fsm->putChipFrame(frameId, chipIndex, frame);
          frame = nullptr;
      }
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
        chipId = int((pixelword & 0xffffffff));
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
      endCursor = MPX_PIXEL_COLUMNS - (MPX_PIXEL_COLUMNS % pixels_per_word);
      assert (frame == nullptr);
      frame = fsm->newChipFrame();
      frame->omr = omr;
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
