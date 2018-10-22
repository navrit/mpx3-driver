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
    _decode( true ),
    _compress( false ),
    _flush( false ),
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
      _mpx3Rx6BitsEnc[i] = pixcode;
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
      _mpx3Rx12BitsEnc[i] = pixcode;
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
}

// ----------------------------------------------------------------------------

bool FramebuilderThread::hasFrame( unsigned long timeout_ms )
{
    return ! pFrameSetManager->isEmpty();
}

// ----------------------------------------------------------------------------

FrameSet *FramebuilderThread::frameData() {
    return pFrameSetManager->getFrameSet();
}

// ----------------------------------------------------------------------------


void FramebuilderThread::releaseFrame(FrameSet * fs) {
    pFrameSetManager->releaseFrameSet(fs);
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

std::string FramebuilderThread::errString()
{
  if( _errString.isEmpty() ) return std::string( "" );
  QString qs = "Framebuilder: " + _errString;
  return qs.toStdString();
}

// ----------------------------------------------------------------------------

int FramebuilderThread::mpx3RawToPixel( unsigned char *raw_bytes,
                                        int            nbytes,
                                        int            chipIndex)
{
  return 0;
}

// ----------------------------------------------------------------------------
