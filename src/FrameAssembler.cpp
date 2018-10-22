#include "FrameAssembler.h"
#include "UdpReceiver.h"
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
      int missing = MPX_PIXEL_COLUMNS - cursor;
      int last_row = row_counter,
              next_row = 0;
      uint16_t next_cursor = 0;
      switch (packetType(pixel_packet[i])) {
        case PIXEL_DATA_SOR :
          // OK, that can happen on position 0, store the row, claim we finished the previous
          next_row = extractRow(lutBugFix(pixel_packet[endCursor/pixels_per_word])) - 1;
          missing = (next_row - last_row) * MPX_PIXEL_COLUMNS;
          assert (next_row >= 0 && next_row < MPX_PIXEL_ROWS);
          break;
        case PIXEL_DATA_SOF :
        case INFO_HEADER_SOF:
        case INFO_HEADER_MID:
        case INFO_HEADER_EOF:
          // somehow we found a new frame, store the current row/frame
          if (frame != nullptr) {
              frame->pixelsLost += missing + (MPX_PIXEL_ROWS - last_row) * MPX_PIXEL_COLUMNS;
              fsm->putChipFrame(chipIndex, frame);
          }
          frame = nullptr;
          break;
        case PIXEL_DATA_MID:
          while (i < packetSize && packetType(pixel_packet[i]) == PIXEL_DATA_MID) i++;
          [[fallthrough]];
        case PIXEL_DATA_EOR :
        case PIXEL_DATA_EOF :
          assert (i < packetSize);
          next_row = extractRow(lutBugFix(pixel_packet[i]));
          assert (next_row > 0 && next_row < MPX_PIXEL_ROWS);
          next_cursor = endCursor - i * pixels_per_word;
      }

      if (next_row > 0) {
          if (frame == nullptr || next_row < row_counter) {
            // we lost the rest of the frame; finish the current and start a new one
            if (frame != nullptr) {
                frame->pixelsLost += missing + (MPX_PIXEL_ROWS - last_row) * MPX_PIXEL_COLUMNS;
                fsm->putChipFrame(chipIndex, frame);
            }
            frame = fsm->newChipFrame(chipIndex);
            missing = 0;
            last_row = -1;
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
          cursor = next_cursor;
          assert (cursor >= 0 && cursor < MPX_PIXEL_COLUMNS);
          assert (packetEndsRow(pixel_packet[(endCursor - cursor) / pixels_per_word]));
          row_counter = next_row;
          frame->pixelsLost += missing + (row_counter - 1 - last_row) * MPX_PIXEL_COLUMNS + cursor;
          if (cursor == 0) {
            assert (packetType(pixel_packet[0]) == PIXEL_DATA_SOR);
            row_counter--;	// the normal processing will start with incrementing and getting the row
          } else {
            row = frame->getRow(row_counter);
          }
      }
    }

  //! Start processing the pixel packet
  //row_number_from_packet = -1;
  for (unsigned j = 0; j < packetSize; ++j, ++pixel_packet) {
    uint64_t pixelword = *pixel_packet;
    uint64_t type = pixelword & PKT_TYPE_MASK;

    switch (type) {
    case PIXEL_DATA_SOF:
      row_counter = -1;
      [[fallthrough]];
    case PIXEL_DATA_SOR:
      ++row_counter;
      assert (row_counter >= 0 && row_counter < MPX_PIXEL_ROWS);
      row = frame->getRow(row_counter);
      cursor = 0;
      [[fallthrough]];
    case PIXEL_DATA_MID:
#ifdef SKIPMOSTPIXELS
      cursor += pixels_per_word;
#else
      for(int k=0; k<pixels_per_word; ++k, ++cursor ) {
        row[cursor] = uint16_t(pixelword & pixel_mask);
        pixelword >>= counter_bits;
      }
#endif
      assert (cursor < MPX_PIXEL_COLUMNS);
      break;
    case PIXEL_DATA_EOF:
      row_counter = -1;
      [[fallthrough]];
    case PIXEL_DATA_EOR:
      pixelword = lutBugFix(pixelword);
      if (type == PIXEL_DATA_EOF) {
          frameId = extractFrameId(pixelword);
      }
      for (; cursor < MPX_PIXEL_COLUMNS; cursor++) {
        row[cursor] = uint16_t(pixelword & pixel_mask);
        pixelword >>= counter_bits;
      }
      if (type == PIXEL_DATA_EOF) {
          // we're done with this one!
          fsm->putChipFrame(chipIndex, frame);
          frame = nullptr;
      }
      break;
    case INFO_HEADER_SOF:
      infoIndex = 0; break;
    case INFO_HEADER_MID:
      if (infoIndex == 4)
        chipId = int((pixelword & 0xffffffff));
      else if (infoIndex == 5 && chipId != 0) {
        omr.setHighR(pixelword & 0xffff);
      }
      infoIndex++; break;
    case INFO_HEADER_EOF:
      if (chipId < 5) break;
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
      frame = fsm->newChipFrame(chipIndex);
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
      break;
    }
  }
}

uint64_t FrameAssembler::lutBugFix(uint64_t pixelword) {
  if (_lutBug) {
      // the pixel word is mangled, un-mangle it
      if (counter_bits == 12) {
          long mask0 = 0x0000000000000fffL,
               mask4 = 0x0fff000000000000L;
          long p0 = _mpx3Rx12BitsLut[pixelword & mask0],
               p4 = ((long) (_mpx3Rx12BitsEnc[(pixelword & mask4) >> 48])) << 48;
          pixelword = (pixelword & (~(mask0 | mask4))) | p0 | p4;
      } else if (counter_bits == 6) {
          // decode pixel 0 .. 3
          long mask0 = 0x000000000000003fL, maskShifted = mask0;
          long wordShifted = pixelword;
          for (int i = 0; i < 4; i++) {
              long pixel = _mpx3Rx6BitsLut[wordShifted & mask0];
              pixelword = (pixelword & (~maskShifted)) | (pixel << (6 * i));
              wordShifted >>= 6;
              maskShifted <<= 6;
          }
          // leave pixel 4 .. 5
          wordShifted >>= 12;
          maskShifted <<= 12;
          // encode pixel 6 .. 9
          for (int i = 6; i < 10; i++) {
              long pixel = _mpx3Rx6BitsEnc[wordShifted & mask0];
              pixelword = (pixelword & (~maskShifted)) | (pixel << (6 * i));
              wordShifted >>= 6;
              maskShifted <<= 6;
          }
      }
  }
  return pixelword;
}
void FrameAssembler::lutInit(bool lutBug) {

    _lutBug = lutBug;
    if (lutBug) {
      // Generate the 6-bit look-up table (LUT) for Medipix3RX decoding
      int pixcode = 0;
      for(int i=0; i<64; i++ )
        {
          _mpx3Rx6BitsLut[pixcode] = i;
          _mpx3Rx6BitsEnc[i] = pixcode;
          // Next code = (!b0 & !b1 & !b2 & !b3 & !b4) ^ b4 ^ b5
          int bit = (pixcode & 0x01) ^ ((pixcode & 0x20)>>5);
          if( (pixcode & 0x1F) == 0 ) bit ^= 1;
          pixcode = ((pixcode << 1) | bit) & 0x3F;
        }

      // Generate the 12-bit look-up table (LUT) for Medipix3RX decoding
      pixcode = 0;
      for(int i=0; i<4096; i++ )
        {
          _mpx3Rx12BitsLut[pixcode] = i;
          _mpx3Rx12BitsEnc[i] = pixcode;
          // Next code = (!b0 & !b1 & !b2 & !b3 & !b4& !b5& !b6 & !b7 &
          //              !b8 & !b9 & !b10) ^ b0 ^ b3 ^ b5 ^ b11
          int bit = ((pixcode & 0x001) ^ ((pixcode & 0x008)>>3) ^
                 ((pixcode & 0x020)>>5) ^ ((pixcode & 0x800)>>11));
          if( (pixcode & 0x7FF) == 0 ) bit ^= 1;
          pixcode = ((pixcode << 1) | bit) & 0xFFF;
        }
    }
}

int   FrameAssembler::_mpx3Rx6BitsLut[64];
int   FrameAssembler::_mpx3Rx6BitsEnc[64];
int   FrameAssembler::_mpx3Rx12BitsLut[4096];
int   FrameAssembler::_mpx3Rx12BitsEnc[4096];
bool  FrameAssembler::_lutBug;
