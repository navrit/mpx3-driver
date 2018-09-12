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
      // If necessary wait until the previous frame has been consumed
      _mutex.lock();
      if( _hasFrame ) _outputCondition.wait( &_mutex );
      _mutex.unlock();


      if( _abortFrame ) return; // Bail out

      // The following decoding operations could be done
      // in separate threads (i.e. by QtConcurrent)
#ifdef _USE_QTCONCURRENT
      if( _n > 1 )
        {
          QFuture<int> qf[4];
          for( i=0; i<_n; ++i )
            qf[i] = QtConcurrent::run( this,
                                       &FramebuilderThreadC::mpx3RawToPixel,
                                       _receivers[i]->frameData(),
                                       _receivers[i]->dataSizeFrame(),
                                       &_decodedFrame[i][0],
                                       _evtHdr.pixelDepth,
                                       //_devHdr[i].deviceType,
                                       //_compress );
                                       _receivers[i]->isCounterhFrame() );
          // Wait for threads to finish and get the results...
          for( i=0; i<_n; ++i )
            _frameSz[i] = qf[i].result();
        }
      else
#endif // _USE_QTCONCURRENT
        {
          for( i=0; i<_n; ++i )
            _frameSz[i] =
              this->mpx3RawToPixel( _receivers[i]->frameData(),
                                    _receivers[i]->dataSizeFrame(),
                                    _decodedFrame[i],
                                    _evtHdr.pixelDepth,
                                    //_devHdr[i].deviceType,
                                    //_compress );
                                    _receivers[i]->isCounterhFrame() );
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

          _isCounterhFrame[i] = _receivers[i]->isCounterhFrame();

          _lostCountFrame[i] = _receivers[i]->pixelsLostFrame();
        }

      ++_framesProcessed;
      if( _evtHdr.pixelDepth != 24 ||
          (_evtHdr.pixelDepth == 24 && this->isCounterhFrame()) )
        {
          _hasFrame = true;
          //if( _callbackFunc ) _callbackFunc( _id );
          _frameAvailableCondition.wakeOne();
        }
    }
}

// ----------------------------------------------------------------------------

int FramebuilderThreadC::mpx3RawToPixel( unsigned char *raw_bytes,
                                         int            nbytes,
                                         int           *pixels,
                                         int            counter_depth,
                                         //int          device_type,
                                         bool           is_counterh )//compress)
{
  // Translate Compact-SPIDR MPX3 data stream
  // into n-bits pixel values in array 'pixel' (with n=counter_depth)
  int counter_bits, pix_per_word, pixel_mask;

  if( counter_depth <= 12 )
    counter_bits = counter_depth;
  else
    counter_bits = 12;

  pix_per_word = 60/counter_bits;
  pixel_mask   = (1<<counter_bits)-1;

  // Parse and unpack the pixel packets
  int  temp[MPX_PIXEL_COLUMNS]; // Temporary storage for a pixel row
  //int *pixelrow = &pixels[0];
  int  index    = 0;
  int  rownr    = -1;
  u64 *pixelpkt = (u64 *) raw_bytes;
  u64  pixelword;
  u64  type;
  int  i, j;
  for( i=0; i<nbytes/sizeof(u64); ++i, ++pixelpkt )
    {
      pixelword = *pixelpkt;
      type = pixelword & PKT_TYPE_MASK;
      switch( type )
        {
        case PIXEL_DATA_SOR:
        case PIXEL_DATA_SOF:
          //++rownr;
          //pixelrow = &pixels[rownr * MPX_PIXEL_COLUMNS];
          index = 0;
          memset( temp, 0, MPX_PIXEL_COLUMNS );
          // 'break' left out intentionally;
          // continue unpacking the pixel packet

        [[fallthrough]]; case PIXEL_DATA_MID:
          // Unpack the pixel packet
          for( j=0; j<pix_per_word; ++j, ++index )
            {
              // Make sure not to write outside the current pixel row
              if( index < MPX_PIXEL_COLUMNS )
                //pixelrow[index] = pixelword & pixel_mask;
                temp[index] = pixelword & pixel_mask;
              pixelword >>= counter_bits;
            }
          break;

        case PIXEL_DATA_EOR:
        case PIXEL_DATA_EOF:
          if( _applyLut )
            // Extract the row counter from the data
            rownr = (int) ((pixelword & ROW_COUNT_MASK) >> ROW_COUNT_SHIFT);
          else
            // The SPIDR LUT erroneously 'decodes' the row counter too..
            // (firmware to be fixed?), so just keep a counter
            ++rownr;

          // Unpack the pixel packet
          for( j=0; j<pix_per_word; ++j, ++index )
            {
              // Make sure not to write outside the current pixel row
              if( index < MPX_PIXEL_COLUMNS )
                //pixelrow[index] = pixelword & pixel_mask;
                temp[index] = pixelword & pixel_mask;
              pixelword >>= counter_bits;
            }

          // Pixel row should be complete now:
          // Copy the row (compiled in the temporary array)
          // to its proper location in the frame
          if( counter_depth == 24 && is_counterh )
            {
              // These frame pixels contains the upper 12 bits from CounterH
              // of the 24-bit pixels, already containing 12 bits from CounterL
              int *p = &pixels[rownr * MPX_PIXEL_COLUMNS];
              for( j=0; j<MPX_PIXEL_COLUMNS; ++j, ++p )
                *p |= (temp[j] << 12);
            }
          else
            {
              memcpy( &pixels[rownr * MPX_PIXEL_COLUMNS], temp,
                      MPX_PIXEL_COLUMNS * sizeof(int) );
            }
          break;

        default:
          // Skip this packet
          break;
        }
    }

  // If necessary, apply a look-up table (LUT)
  //if( device_type == MPX_TYPE_MPX3RX && counter_depth > 1 && _applyLut )
  if( counter_depth > 1 && _applyLut )
    {
      // Medipix3RX device: apply LUT
      if( counter_depth == 6 )
        {
          for( int i=0; i<MPX_PIXELS; ++i )
            pixels[i] = _mpx3Rx6BitsLut[pixels[i] & 0x3F];
        }
      else if( counter_depth == 12 )
        {
          for( int i=0; i<MPX_PIXELS; ++i )
            pixels[i] = _mpx3Rx12BitsLut[pixels[i] & 0xFFF];
        }
      else if( counter_depth == 24 && is_counterh )
        {
          int pixval;
          for( int i=0; i<MPX_PIXELS; ++i )
            {
              pixval     = pixels[i];
              // Lower 12 bits
              pixels[i]  = _mpx3Rx12BitsLut[pixval & 0xFFF];
              // Upper 12 bits
              pixval     = (pixval >> 12) & 0xFFF;
              pixels[i] |= (_mpx3Rx12BitsLut[pixval] << 12);
            }
        }
    }

  // Return a size in bytes
  int size = MPX_PIXELS * sizeof(int);
  //if( !compress ) return size;

  // Compress 4- and 6-bit frames into 1 byte per pixel and 12-bit frames
  // into 2 bytes per pixel and 1-bit frames into 1 bit per pixel
  // if( counter_depth == 12 )
  //   {
  //     u16 *pixels16 = (u16 *) pixels;
  //     int *pixels32 = (int *) pixels;
  //     for( int i=0; i<MPX_PIXELS; ++i, ++pixels16, ++pixels32 )
  //       *pixels16 = (u16) ((*pixels32) & 0xFFFF);
  //     size = MPX_PIXELS * sizeof( u16 );
  //   }
  // else if( counter_depth == 4 || counter_depth == 6 )
  //   {
  //     u8  *pixels8  = (u8 *)  pixels;
  //     int *pixels32 = (int *) pixels;
  //     for( int i=0; i<MPX_PIXELS; ++i, ++pixels8, ++pixels32 )
  //       *pixels8 = (u8) ((*pixels32) & 0xFF);
  //     size = MPX_PIXELS * sizeof( u8 );
  //   }
  // else if( counter_depth == 1 )
  //   {
  //     int *pixels1  = (int *) pixels;
  //     int *pixels32 = (int *) pixels;
  //     int  pixelword = 0;
  //     for( int i=0; i<MPX_PIXELS; ++i, ++pixels32 )
  //       {
  //         if( (*pixels32) & 0x1 )
  //           pixelword |= 1 << (i&31);
  //         if( (i & 31) == 31 )
  //           {
  //             *pixels1 = pixelword;
  //             pixelword = 0;
  //             ++pixels1;
  //           }
  //       }
  //     size = MPX_PIXELS / 8;
  //   }
  return size;
}

// ----------------------------------------------------------------------------
