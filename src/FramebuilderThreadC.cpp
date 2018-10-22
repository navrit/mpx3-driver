#include "FramebuilderThreadC.h"
#include "ReceiverThreadC.h"
#include "mpx3defs.h"

#define _USE_QTCONCURRENT
#ifdef _USE_QTCONCURRENT
#if QT_VERSION >= 0x050000
#include <QtConcurrent>
#else
#include <QtCore>
#endif
#endif

// ----------------------------------------------------------------------------

FramebuilderThreadC::FramebuilderThreadC( std::vector<ReceiverThread *> recvrs,
                                        QObject *parent )
  : FramebuilderThread( recvrs, parent )
{
}

// ----------------------------------------------------------------------------

FramebuilderThreadC::~FramebuilderThreadC()
{
}

// ----------------------------------------------------------------------------
// NB: processFrame() identical to FramebuilderThread::processFrame()
//     except for the function name for QtConcurrent::run().
//     Note that the implementation of mpx3RawTpPixel() *is* indeed
//     completely different from the implemention in FramebuilderThread.

void FramebuilderThreadC::processFrame()
{
  // Not writing to file, so if not flushing it all,
  // we expect at least to decode the pixel data
  // otherwise just 'absorb' the frames...
  unsigned int i;
  if( !_flush && _decode )
    {

      if( _abortFrame ) return; // Bail out

      // The following decoding operations could be done
      // in separate threads (i.e. by QtConcurrent)
#ifdef _USE_QTCONCURRENT
      int chipmask = (1 << _n) - 1;

      if( _n > 1 ) do {

          QFuture<int> qf[4];
          for( i=0; i<_n; ++i )
              if (((1 << i) & chipmask) != 0)
                qf[i] = QtConcurrent::run( this,
                                       &FramebuilderThreadC::mpx3RawToPixel,
                                       _receivers[i]->frameData(),
                                       _receivers[i]->dataSizeFrame(),
                                       i);
          // Wait for threads to finish and get the results...
          int8_t deltas[4];
          bool jump = false;
          for( i=0; i<_n; ++i )
              if (((1 << i) & chipmask) != 0) {
                int oldId = _frameId[i];
                int newId = _frameId[i] = qf[i].result();
                int8_t delta = deltas[i] = (int8_t) (newId - oldId);
                if (delta != 1) jump = true;
          }
          if (jump) {
              qDebug() << "[WARNING] FrameIds jump: " << deltas[0] << ' ' << deltas[1] << ' ' << deltas[2] << ' ' << deltas[3];
          }
          bool different = false;
          int maxid = -1;
          for (int i = 0; i < _n; ++i) {

            int fid = _frameId[i];
            if (fid != maxid) {
                if (maxid == -1) {
                    maxid = fid;
                    different = i>0;
                } else {
                    if (fid >= 0 && ((int8_t) (fid - maxid)) > 0) maxid = fid;
                    different = true;
                }
            }
          }
          chipmask = 0;
          if (different) {
              qDebug() << "[WARNING] FrameIds " << _frameId[0] << ' ' << _frameId[1] << ' ' << _frameId[2] << ' ' << _frameId[3];
              for (int i = 0; i < _n; ++i) {
                  if (_frameId[i] != maxid) {
                      _receivers[i]->releaseFrame();
                      _mutex.lock();
                      while( !_receivers[i]->hasFrame() ) _inputCondition.wait( &_mutex );
                      _mutex.unlock();
                      chipmask |= (1 << i);
                  }
              }
          } else if (maxid == -1) {
              chipmask = (1 << _n) - 1;
          }
      }
          while (chipmask != 0);
      else
#endif // _USE_QTCONCURRENT
        {
          for( i=0; i<_n; ++i )
              this->mpx3RawToPixel( _receivers[i]->frameData(),
                                    _receivers[i]->dataSizeFrame(),
                                    i);
        }

      // Collect various information on the frames from their receivers
      _timeStamp = _receivers[0]->timeStampFrame();
      _timeStampSpidr = _receivers[0]->timeStampFrameSpidr();
      for( i=0; i<_n; ++i )
        {
          // Copy (part of) the SPIDR 'header' (6 short ints: 3 copied)
          memcpy( (void *) &_spidrHeader[i],
                  (void *) _receivers[i]->spidrHeaderFrame(),
                  SPIDR_HEADER_SIZE/2 ); // Only half of it needed here
        }

      ++_framesProcessed;
    }
}

// ----------------------------------------------------------------------------

int FramebuilderThreadC::mpx3RawToPixel( unsigned char *raw_bytes,
                                         int            nbytes,
                                         int            chipIndex)
{
  // Translate Compact-SPIDR MPX3 data stream
  // into n-bits pixel values in array 'pixel' (with n=counter_depth)
  int counter_depth, counter_bits, pix_per_word, pixel_mask;

  ChipFrame *frame = nullptr;

  //int *pixelrow = &pixels[0];
  int  index    = 0;
  int  rownr    = -1;
  u64 *pixelpkt = (u64 *) raw_bytes;
  u64  pixelword;
  u64  type;
  int  i, j;
  uint16_t *temp2 = nullptr;
  int frameId = -1;
  int infoIndex, chipId = 0;
  OMR omr;
  for( i=0; i<nbytes/sizeof(u64); ++i, ++pixelpkt )
    {
      pixelword = *pixelpkt;
      type = pixelword & PKT_TYPE_MASK;
      switch( type )
        {
        case PIXEL_DATA_SOR:
        case PIXEL_DATA_SOF:
          ++rownr;
          //pixelrow = &pixels[rownr * MPX_PIXEL_COLUMNS];
          index = 0;
          temp2 = frame->getRow(rownr);
          // 'break' left out intentionally;
          // continue unpacking the pixel packet

        [[fallthrough]]
        case PIXEL_DATA_MID:
          // Unpack the pixel packet
          // Make sure not to write outside the current pixel row
        { int maxj = MPX_PIXEL_COLUMNS - index;
          if (maxj > pix_per_word) maxj = pix_per_word;
          for( j=0; j<maxj; ++j, ++index )
            {
              //pixelrow[index] = pixelword & pixel_mask;
              temp2[index] = pixelword & pixel_mask;
              pixelword >>= counter_bits;
            }
         }
          break;

        case PIXEL_DATA_EOF:
          frameId = (int) ((pixelword & FRAME_FLAGS_MASK) >> FRAME_FLAGS_SHIFT);

        case PIXEL_DATA_EOR:
          if (_lutBug && ! _applyLut) {
              // the pixel word is mangled, un-mangle it
              if (counter_bits == 12) {
                  long mask0 = 0x0000000000000fffL,
                       mask4 = 0x0fff000000000000L;
                  long p0 = _mpx3Rx12BitsLut[pixelword & mask0],
                       p4 = ((long) (_mpx3Rx12BitsEnc[(pixelword & mask4) >> 48])) << 48;
                  pixelword = pixelword & (~(mask0 | mask4)) | p0 | p4;
              } else if (counter_bits == 6) {
                  // decode pixel 0 .. 3
                  long mask0 = 0x000000000000003fL, maskShifted = mask0;
                  long wordShifted = pixelword;
                  for (int i = 0; i < 4; i++) {
                      long pixel = _mpx3Rx6BitsLut[wordShifted & mask0];
                      pixelword = pixelword & (~maskShifted) | (pixel << (6 * i));
                      wordShifted >>= 6;
                      maskShifted <<= 6;
                  }
                  // leave pixel 4 .. 5
                  wordShifted >>= 12;
                  maskShifted <<= 12;
                  // encode pixel 6 .. 9
                  for (int i = 6; i < 10; i++) {
                      long pixel = _mpx3Rx6BitsEnc[wordShifted & mask0];
                      pixelword = pixelword & (~maskShifted) | (pixel << (6 * i));
                      wordShifted >>= 6;
                      maskShifted <<= 6;
                  }
              }
              if (type == PIXEL_DATA_EOF) {
                  // redo that!
                  frameId = (int) ((pixelword & FRAME_FLAGS_MASK) >> FRAME_FLAGS_SHIFT);
              }
          }
            // We could extract the row counter from the data
            // except MOST old firmware versions have a LUT that
            // erroneously 'decodes' the row counter too..
            // NB: the above is an old comment, the block above should actually work around that bug! (BB/181002)
            // assert rownr === (int) ((pixelword & ROW_COUNT_MASK) >> ROW_COUNT_SHIFT);

          // Unpack the pixel packet
          for( j=0; j<pix_per_word; ++j, ++index )
            {
              // Make sure not to write outside the current pixel row
              if( index < MPX_PIXEL_COLUMNS )
                //pixelrow[index] = pixelword & pixel_mask;
                temp2[index] = pixelword & pixel_mask;
              pixelword >>= counter_bits;
            }
          if (type == PIXEL_DATA_EOF) {
              frame->frameId = frameId;
              frame->pixelsLost = _receivers[chipIndex]->pixelsLostFrame();
              pFrameSetManager->putChipFrame(chipIndex, frame);
          }

          break;

          case INFO_HEADER_SOF:
            infoIndex = 0; chipId = 0; break;
          case INFO_HEADER_MID:
            if (infoIndex == 4)
              chipId = int((pixelword & 0xffffffff));
            else if (infoIndex == 5 && chipId > 1000) {
              omr.setHighR(pixelword & 0xffff);
            }
            infoIndex++; break;
          case INFO_HEADER_EOF:
            if (chipId > 1000) {
                omr.setLowR(pixelword & 0xffffffff);
                switch (omr.getCountL()) {
                  case 0: counter_depth = 1; break;
                  case 1: counter_depth = 6; break;
                  case 2: counter_depth = 12; break;
                  case 3: counter_depth = 24; break;
                }
                //qDebug() << " depth " << counter_depth << " mode " << omr.getMode();
                counter_bits = counter_depth == 24 ? 12 : counter_depth;
                pix_per_word = 60 / counter_bits;
                pixel_mask = (1 << counter_bits) - 1;
                //endCursor = MPX_PIXEL_COLUMNS - (MPX_PIXEL_COLUMNS % pix_per_word);
                //assert (frame == nullptr);
                frame = pFrameSetManager->newChipFrame(chipIndex);
                frame->omr = omr;
            }
            break;

        default:
          // Skip this packet
          break;
        }
    }

  return frameId;
}

// ----------------------------------------------------------------------------
