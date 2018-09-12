#include "FramebuilderThread.h"
#include "ReceiverThread.h"
#include "mpx3defs.h"

#define _USE_QTCONCURRENT
#ifdef _USE_QTCONCURRENT
#if QT_VERSION >= 0x050000
#include <QtConcurrent>
#else
#include <QtCore>
#endif
#endif

#define byteswap(x) ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))

// ----------------------------------------------------------------------------

FramebuilderThread::FramebuilderThread( std::vector<ReceiverThread *> recvrs,
                                        QObject *parent )
  : QThread( parent ),
    _receivers( recvrs ),
    _stop( false ),
    _id( 0 ),
    _callbackFunc( 0 ),
    _framesReceived( 0 ),
    _framesWritten( 0 ),
    _framesProcessed( 0 ),
    _lostCountTotal( 0 ),
    _decode( false ),
    _compress( false ),
    _flush( false ),
    _hasFrame( false ),
    _abortFrame( false ),
    _fileOpen( false ),
    _applyLut( true ),
    _timeStamp( 0 ),
    _timeStampSpidr( 0 )
{
  u32 i;
  _n = _receivers.size();
  for( i=0; i<NR_OF_DEVICES; ++i )
    {
      _lostCountFrame[i] = 0;
      _frameSz[i]         = 0;
      _isCounterhFrame[i] = false;
      memset( static_cast<void *> (&_spidrHeader[i]), 0, SPIDR_HEADER_SIZE );
    }
  // Preset the headers
  _evtHdr.headerId   = EVT_HEADER_ID;
  _evtHdr.headerSize = EVT_HEADER_SIZE;
  _evtHdr.format     = EVT_HEADER_VERSION;
  for( i=0; i<sizeof(_evtHdr.unused)/sizeof(u32); ++i )
    _evtHdr.unused[i] = HEADER_FILLER_WORD;
  for( i=0; i<sizeof(_evtHdr.triggerConfig)/sizeof(u32); ++i )
    _evtHdr.triggerConfig[i] = 0xAAAAAAAA; // ### Not yet implemented
  for( i=0; i<NR_OF_DEVICES; ++i )
    {
      _devHdr[i].headerId   = DEV_HEADER_ID;
      _devHdr[i].headerSize = DEV_HEADER_SIZE;
      _devHdr[i].format     = DEV_HEADER_VERSION;
      _devHdr[i].deviceId   = (i+1) * 0x11111111; // Dummy ID
      _devHdr[i].deviceType = MPX_TYPE_NC; // Not connected
      for( u32 j=0; j<sizeof(_devHdr[i].unused)/sizeof(u32); ++j )
        _devHdr[i].unused[j] = HEADER_FILLER_WORD;
    }

  // Generate the 6-bit look-up table (LUT) for Medipix3RX decoding
  int pixcode = 0, bit;
  for( i=0; i<64; i++ )
    {
      _mpx3Rx6BitsLut[pixcode] = i;
      // Next code = (!b0 & !b1 & !b2 & !b3 & !b4) ^ b4 ^ b5
      bit = (pixcode & 0x01) ^ ((pixcode & 0x20)>>5);
      if( (pixcode & 0x1F) == 0 ) bit ^= 1;
      pixcode = ((pixcode << 1) | bit) & 0x3F;
    }

  // Generate the 12-bit look-up table (LUT) for Medipix3RX decoding
  pixcode = 0;
  for( i=0; i<4096; i++ )
    {
      _mpx3Rx12BitsLut[pixcode] = i;
      // Next code = (!b0 & !b1 & !b2 & !b3 & !b4& !b5& !b6 & !b7 &
      //              !b8 & !b9 & !b10) ^ b0 ^ b3 ^ b5 ^ b11
      bit = ((pixcode & 0x001) ^ ((pixcode & 0x008)>>3) ^
             ((pixcode & 0x020)>>5) ^ ((pixcode & 0x800)>>11));
      if( (pixcode & 0x7FF) == 0 ) bit ^= 1;
      pixcode = ((pixcode << 1) | bit) & 0xFFF;
    }

  // Start the thread 
  this->start();
}

// ----------------------------------------------------------------------------

FramebuilderThread::~FramebuilderThread()
{
  // In case still running...
  this->stop();
}

// ----------------------------------------------------------------------------

void FramebuilderThread::stop()
{
  if( this->isRunning() )
    {
      _stop = true;
      _abortFrame = true;
      _mutex.lock();
      _inputCondition.wakeOne();
      _outputCondition.wakeOne();
      _mutex.unlock();
      this->wait(); // Wait until this thread (i.e. function run()) exits
    }
}

// ----------------------------------------------------------------------------

void FramebuilderThread::run()
{
  if( _receivers.empty() ) _stop = true;

  u32 i;
  while( !_stop )
    {
      if( _receivers[0]->hasFrame() )
        {
          // Wait for the other receivers if necessary, before proceeding
          _abortFrame = false; // Allows us to abort waiting for frame data
          for( i=1; i<_n; ++i )
            while( !(_receivers[i]->hasFrame() || _abortFrame) );

          if( _fileOpen )
            this->writeFrameToFile();
          else
            this->processFrame();

          // Release this frame buffer on all receivers
          // (###NB: how do we prevent 1 receiver to start filling
          //  the newly released buffer while another receiver
          //  still has a full buffer and starts (again) overwriting
          //  its last received frame, potentially causing desynchronized
          //  event buffers in the various receivers?
          //  A task for a busy/inhibit mechanism?: set busy before
          //  filling up any buffer)
          for( i=0; i<_n; ++i ) _receivers[i]->releaseFrame();

          ++_framesReceived;
        }
      else
        {
          _mutex.lock();
          if( !_receivers[0]->hasFrame() ) _inputCondition.wait( &_mutex );
          _mutex.unlock();
        }
    }
}

// ----------------------------------------------------------------------------

void FramebuilderThread::inputNotification()
{
  // There should be input data, so check for it in run()
  // (Mutex to make sure that the 'wake' does not occur just in front
  //  of the 'wait' in run())
  _mutex.lock();
  _inputCondition.wakeOne();
  _mutex.unlock();
}

// ----------------------------------------------------------------------------

void FramebuilderThread::abortFrame()
{
  _abortFrame = true;

  // In case the thread is waiting in 'processFrame()'
  _mutex.lock();
  _outputCondition.wakeOne();
  _mutex.unlock();
}

// ----------------------------------------------------------------------------

void FramebuilderThread::processFrame()
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
                                       &FramebuilderThread::mpx3RawToPixel,
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

          _lostCountFrame[i] = _receivers[i]->packetsLostFrame();
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

void FramebuilderThread::writeFrameToFile()
{
  if( _decode )
    this->writeDecodedFrameToFile();
  else
    this->writeRawFrameToFile();

  _file.flush();
  ++_framesWritten;
}

// ----------------------------------------------------------------------------

void FramebuilderThread::writeRawFrameToFile()
{
  // Get and format the data for this frame from all receivers
  u32 i, sz, evt_sz;

  // Fill the headers with the frame data sizes
  // (NB: in fact the size is known beforehand from the selected pixel depth)
  evt_sz = _n * DEV_HEADER_SIZE;
  for( i=0; i<_n; ++i )
    {
      sz = _receivers[i]->dataSizeFrame();
      _devHdr[i].dataSize = sz;
      evt_sz += sz;
    }
  _evtHdr.dataSize = evt_sz;

  // Fill in the rest of the event header and write it
  i64 timestamp = _receivers[0]->timeStampFrame();
  _evtHdr.secs  = (u32) (timestamp / 1000);
  _evtHdr.msecs = (u32) (timestamp % 1000);
  _evtHdr.evtNr = _framesReceived;
  _file.write( (const char *) &_evtHdr, EVT_HEADER_SIZE );

  DevHeader_t *p_devhdr;
  int lost;
  for( i=0; i<_n; ++i )
    {
      // Fill in the rest of the device header and write it
      p_devhdr = &_devHdr[i];
      lost = _receivers[i]->lostCountFrame();
      p_devhdr->lostPackets = lost;
      _lostCountFrame[i]    = lost;
      _lostCountTotal      += lost;

      // Copy the saved SPIDR 'header' (6 short ints)
      memcpy( (void *) p_devhdr->spidrHeader,
              (void *) _receivers[i]->spidrHeaderFrame(),
              SPIDR_HEADER_SIZE );
      _file.write( (const char *) p_devhdr, DEV_HEADER_SIZE );

      // Write the raw frame data of this device
      _file.write( (const char *) _receivers[i]->frameData(),
                   p_devhdr->dataSize );
    }
}

// ----------------------------------------------------------------------------

void FramebuilderThread::writeDecodedFrameToFile()
{
  u32 i;

  // The following decoding operations could be done
  // in separate threads (i.e. by QtConcurrent)
  int frame_sz[4];
  for( i=0; i<_n; ++i )
    frame_sz[i] = this->mpx3RawToPixel( _receivers[i]->frameData(),
                                        _receivers[i]->dataSizeFrame(),
                                        _decodedFrame[i],
                                        _evtHdr.pixelDepth,
                                        //_devHdr[i].deviceType,
                                        //_compress );
                                        _receivers[i]->isCounterhFrame() );

  // Fill the headers with the frame data sizes
  int evt_sz = _n * DEV_HEADER_SIZE;
  for( i=0; i<_n; ++i )
    {
      _devHdr[i].dataSize = frame_sz[i];
      evt_sz += frame_sz[i];
    }
  _evtHdr.dataSize = evt_sz;

  // Fill in the rest of the event header and write it
  i64 timestamp = _receivers[0]->timeStampFrame();
  _evtHdr.secs  = (u32) (timestamp / 1000);
  _evtHdr.msecs = (u32) (timestamp % 1000);
  _evtHdr.evtNr = _framesWritten;
  _file.write( (const char *) &_evtHdr, EVT_HEADER_SIZE );

  DevHeader_t *p_devhdr;
  int lost;
  for( i=0; i<_n; ++i )
    {
      // Fill in the rest of the device header and write it
      p_devhdr = &_devHdr[i];
      lost = _receivers[i]->lostCountFrame();
      p_devhdr->lostPackets = lost;
      _lostCountFrame[i]    = lost;
      _lostCountTotal      += lost;

      // Copy the saved SPIDR 'header' (6 short ints)
      memcpy( (void *) p_devhdr->spidrHeader,
              (void *) _receivers[i]->spidrHeaderFrame(),
              SPIDR_HEADER_SIZE );
      _file.write( (const char *) p_devhdr, DEV_HEADER_SIZE );

      // Write the decoded frame data of this device
      _file.write( (const char *) _decodedFrame[i], frame_sz[i] );
    }
}

// ----------------------------------------------------------------------------

bool FramebuilderThread::hasFrame( unsigned long timeout_ms )
{
  if( timeout_ms == 0 ) return _hasFrame;

  // For timeout_ms > 0
  _mutex.lock();
  if( !_hasFrame )
    _frameAvailableCondition.wait( &_mutex, timeout_ms );
  _mutex.unlock();
  return _hasFrame;
}

// ----------------------------------------------------------------------------

int *FramebuilderThread::frameData( int  index,
                                    int *size,
                                    int *lost_count )
{
  if( _hasFrame )
    *size = _frameSz[index];
  else
    *size = 0;

  if( lost_count ) *lost_count = _lostCountFrame[index];

  return &_decodedFrame[index][0];
}

// ----------------------------------------------------------------------------

void FramebuilderThread::clearFrameData( int index )
{
  index &= 0x3;
  memset( static_cast<void *> (_decodedFrame[index]),
          0, MPX_PIXELS * sizeof(int) );
}

// ----------------------------------------------------------------------------

void FramebuilderThread::releaseFrame()
{
  _mutex.lock();
  _hasFrame = false;
  _outputCondition.wakeOne();
  _mutex.unlock();
}

// ----------------------------------------------------------------------------

i64 FramebuilderThread::frameTimestamp()
{
  return _timeStamp;
}

// ----------------------------------------------------------------------------

double FramebuilderThread::frameTimestampDouble()
{
  u32 secs  = (u32) (_timeStamp / (i64)1000);
  u32 msecs = (u32) (_timeStamp % (i64)1000);
  return( (double) secs + ((double) msecs)/1000.0 );
}

// ----------------------------------------------------------------------------

i64 FramebuilderThread::frameTimestampSpidr()
{
  return _timeStampSpidr;
}

// ----------------------------------------------------------------------------

int FramebuilderThread::frameShutterCounter( int index )
{
  if( index < 0 )
    {
      // Return the shutter counter if identical for each device,
      // otherwise return -1
      int cnt = (int) _spidrHeader[0].shutterCnt;
      for( u32 i=1; i<_n; ++i )
        if( cnt != (int) _spidrHeader[i].shutterCnt )
          return -1;
      return byteswap( cnt );
    }
  else
    {
      index &= 0x3;
      return byteswap( (int) _spidrHeader[index].shutterCnt );
    }
}

// ----------------------------------------------------------------------------

bool FramebuilderThread::isCounterhFrame( int index )
{
  if( index < 0 )
    {
      // Return true if each device has a 'CounterH' frame,
      // otherwise return false
      bool b = _isCounterhFrame[0];
      for( u32 i=1; i<_n; ++i )
        if( b != _isCounterhFrame[i] )
          return false;
      return b;
    }
  else
    {
      index &= 0x3;
      return _isCounterhFrame[index];
    }
}

// ----------------------------------------------------------------------------

void FramebuilderThread::setAddrInfo( int *ipaddr,
                                      int *ports )
{
  _evtHdr.ipAddress = ((ipaddr[0] << 0)  | (ipaddr[1] << 8) |
                       (ipaddr[2] << 16) | (ipaddr[3] << 24));
  u32 ndevs = 0;
  for( int i=0; i<4; ++i )
    {
      _evtHdr.ports[i] = ports[i];
      if( ports[i] > 0 ) ++ndevs;
    }
  _evtHdr.nrOfDevices = ndevs;
}

// ----------------------------------------------------------------------------

void FramebuilderThread::setDeviceIdsAndTypes( int *ids, int *types )
{
  // _devHdr is only used for as many devices as there are,
  // irrespective of their position 'i'
  int i, index = 0;
  for( i=0; i<4; ++i )
    if( ids[i] != 0 )
      {
        _devHdr[index].deviceId   = ids[i];
        _devHdr[index].deviceType = types[i];
        ++index;
      }
}

// ----------------------------------------------------------------------------

void FramebuilderThread::setPixelDepth( int nbits )
{
  _evtHdr.pixelDepth = nbits;
}

// ----------------------------------------------------------------------------

void FramebuilderThread::setDecodeFrames( bool decode )
{
  _decode = decode;
  for( int i=0; i<4; ++i )
    {
      if( decode )
        _devHdr[i].format |= DEV_DATA_DECODED;
      else
        _devHdr[i].format &= ~DEV_DATA_DECODED;
    }
}

// ----------------------------------------------------------------------------

void FramebuilderThread::setCompressFrames( bool compress )
{
  _compress = compress;
  for( int i=0; i<4; ++i )
    {
      if( compress )
        _devHdr[i].format |= DEV_DATA_COMPRESSED;
      else
        _devHdr[i].format &= ~DEV_DATA_COMPRESSED;
    }
}

// ----------------------------------------------------------------------------

bool FramebuilderThread::openFile( std::string filename, bool overwrite )
{
  this->closeFile();

  QString fname = QString::fromStdString( filename );
  if( QFile::exists( fname ) && !overwrite )
    {
      _errString = "File \"" + fname + "\" already exists";
      return false;
    }

  _file.setFileName( fname );
  _file.open( QIODevice::WriteOnly );
  if( _file.isOpen() )
    {
      _framesWritten = 0;
      _lostCountTotal = 0;
      _fileOpen = true;
      return true;
    }
  _errString = "Failed to open file \"" + fname + "\"";  
  return false;
}

// ----------------------------------------------------------------------------

bool FramebuilderThread::closeFile()
{
  if( _file.isOpen() )
    {
      _file.close();
      _fileOpen = false;
      return true;
    }
  return false;
}

// ----------------------------------------------------------------------------

int FramebuilderThread::lostCountFrame()
{
  int lost = 0;
  for( u32 i=0; i<_n; ++i )
    lost += _lostCountFrame[i];
  return lost;
}

// ----------------------------------------------------------------------------

std::string FramebuilderThread::errString()
{
  if( _errString.isEmpty() ) return std::string( "" );
  QString qs = "Framebuilder: " + _errString;
  return qs.toStdString();
}

// ----------------------------------------------------------------------------

int FramebuilderThread::mpx3RawToPixel( unsigned char *raw_bytes,
                                        int            nbytes,
                                        int           *pixels,
                                        int            counter_depth,
                                        //int          device_type,
                                        bool           is_counterh )//compress)
{
  // Convert MPX3 raw bit stream in byte array 'raw_bytes'
  // into n-bits pixel values in array 'pixel' (with n=counter_depth)
  // (NB: parameter 'nbytes' here not used, intentionally)
  // (NB: parameter 'device_type' removed as it appears the function
  //      for QtConcurrent::run can have up to 5 parameters only..)
  int            counter_bits, row, col, offset, pixelbit;
  int            bitmask, bitmask_start;
  int           *ppix;
  unsigned char  byte;
  unsigned char *praw;
  Q_UNUSED( nbytes );

  // Necessary to globally clear the pixels array
  // as we only use '|' (OR) in the assignments below
  // (but not if this frame holds the upper 12 bits of a 24-bit frame read-out!)
  if( !(counter_depth == 24 && is_counterh) )
    memset( static_cast<void *> (pixels), 0, MPX_PIXELS * sizeof(int) );

  // Raw data arrives as: all bits n+1 from 1 row of pixels,
  // followed by all bits n from the same row of pixels, etc.
  // (so bit 'counter-bits-1', the highest bit comes first),
  // until all bits of this pixel row have arrived,
  // then the same happens for the next row of pixels, etc.
  // NB: for 24-bits readout data arrives as:
  // all bits 11 to 0 for the 1st row, bits 11 to 0 for the 2nd row,
  // etc for all 256 rows as for other pixel depths,
  // then followed by bits 23 to 12 for the 1st row, etc,
  // again for all 256 rows.
  if( counter_depth <= 12 )
    counter_bits = counter_depth;
  else
    counter_bits = 12;
  if( counter_depth == 24 && is_counterh )
    bitmask_start = (1 << (24-1));
  else
    bitmask_start = (1 << (counter_bits-1));
  offset = 0;
  praw = raw_bytes;
  for( row=0; row<MPX_PIXEL_ROWS; ++row )
    {
      bitmask = bitmask_start;
      for( pixelbit=counter_bits-1; pixelbit>=0; --pixelbit )
        {
          ppix = &pixels[offset];
          // All bits 'pixelbit' of one pixel row (= 256 pixels or columns)
          for( col=0; col<MPX_PIXEL_COLUMNS; col+=8 )
            {
              // Process raw data byte-by-byte
              byte = *praw;
              if( byte != 0 )
                {
                  /*
                  if( byte & 0x01 ) ppix[0] |= bitmask;
                  if( byte & 0x02 ) ppix[1] |= bitmask;
                  if( byte & 0x04 ) ppix[2] |= bitmask;
                  if( byte & 0x08 ) ppix[3] |= bitmask;
                  if( byte & 0x10 ) ppix[4] |= bitmask;
                  if( byte & 0x20 ) ppix[5] |= bitmask;
                  if( byte & 0x40 ) ppix[6] |= bitmask;
                  if( byte & 0x80 ) ppix[7] |= bitmask;
                  ppix += 8;
                  */
                  // Faster ?
                  if( byte & 0x01 ) *ppix |= bitmask;
                  ++ppix;
                  if( byte & 0x02 ) *ppix |= bitmask;
                  ++ppix;
                  if( byte & 0x04 ) *ppix |= bitmask;
                  ++ppix;
                  if( byte & 0x08 ) *ppix |= bitmask;
                  ++ppix;
                  if( byte & 0x10 ) *ppix |= bitmask;
                  ++ppix;
                  if( byte & 0x20 ) *ppix |= bitmask;
                  ++ppix;
                  if( byte & 0x40 ) *ppix |= bitmask;
                  ++ppix;
                  if( byte & 0x80 ) *ppix |= bitmask;
                  ++ppix;
                }
              else
                {
                  ppix += 8;
                }
              ++praw; // Next raw byte
            }
          bitmask >>= 1;
        }
      offset += MPX_PIXEL_COLUMNS;
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
  int size = (MPX_PIXELS * sizeof(int));
  //if( !compress ) return size;

  /*
  // Compress 4- and 6-bit frames into 1 byte per pixel
  // and 12-bit frames into 2 bytes per pixel (1-bit frames already
  // available 'compressed' into 1 bit per pixel in array 'raw_bytes')
  if( counter_depth == 12 )
    {
      u16 *pixels16 = (u16 *) pixels;
      int *pixels32 = (int *) pixels;
      for( int i=0; i<MPX_PIXELS; ++i, ++pixels16, ++pixels32 )
        *pixels16 = (u16) ((*pixels32) & 0xFFFF);
      size = (MPX_PIXELS * sizeof( u16 ));
    }
  else if( counter_depth == 4 || counter_depth == 6 )
    {
      u8  *pixels8  = (u8 *)  pixels;
      int *pixels32 = (int *) pixels;
      for( int i=0; i<MPX_PIXELS; ++i, ++pixels8, ++pixels32 )
        *pixels8 = (u8) ((*pixels32) & 0xFF);
      size = (MPX_PIXELS * sizeof( u8 ));
    }
  else if( counter_depth == 1 )
    {
      // 1-bit frame: just copy the raw frame over into array 'pixels'
      // so that it becomes a 'decoded' and 'compressed' frame
      memcpy( (void *) pixels, (void *) raw_bytes, MPX_PIXELS/8 );
      size = (MPX_PIXELS / 8);
    }
  */
  return size;
}

// ----------------------------------------------------------------------------
