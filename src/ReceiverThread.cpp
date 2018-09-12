#include <QUdpSocket>
#include <QAbstractSocket>
#ifdef WIN32
#include <winsock2.h>
#else
// Linux
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#endif

#include "ReceiverThread.h"
#include "FramebuilderThread.h"
#include "mpx3defs.h"

#define byteswap(x) ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))

// ----------------------------------------------------------------------------

ReceiverThread::ReceiverThread( int *ipaddr,
				int  port,
				QObject *parent )
  : QThread( parent ),
    _sock( 0 ),
    _port( port ),
    _stop( false ),
    _frameBuilder( 0 ),
    _spidrController( 0 ),
    _expFrameSize( MPX3_12BIT_RAW_SIZE ),
    _expPayloadSize( 0 - SPIDR_HEADER_SIZE ),
    _expPacketsPerFrame( 999 ),
    _currShutterCnt( -1 ),
    _expSequenceNr( 0 ),
    _copySpidrHeader( true ),
    _pixelDepth( 12 ),
    _framesReceived( 0 ),
    _framesLost( 0 ),
    _packetsReceived( 0 ),
    _packetsLost( 0 ),
    _head( 0 ),
    _tail( 0 ),
    _empty( true ),
    _currFrameBuffer( _frameBuffer[0] ),
    _recvTimeoutCount( 0 )
{
  _spidrHeader = (SpidrHeader_t *) _recvBuffer;
  _addr = (((ipaddr[3] & 0xFF) << 24) | ((ipaddr[2] & 0xFF) << 16) |
	   ((ipaddr[1] & 0xFF) << 8) | ((ipaddr[0] & 0xFF) << 0));
  _addrStr = (QString::number( ipaddr[3] ) + '.' +
	      QString::number( ipaddr[2] ) + '.' +
	      QString::number( ipaddr[1] ) + '.' +
	      QString::number( ipaddr[0] ));
  for( u32 i=0; i<NR_OF_FRAMEBUFS; ++i )
    {
      _packetsLostFrame[i] = 9999;
      _isCounterhFrame[i]  = false;
      _timeStamp[i]        = QDateTime();
      // Initialize the frame buffers
      memset( _frameBuffer[i], 0xFF, sizeof(_frameBuffer[0]) );
    }
  this->start();
}

// ----------------------------------------------------------------------------

ReceiverThread::~ReceiverThread()
{
  // In case still running...
  this->stop();
}

// ----------------------------------------------------------------------------

void ReceiverThread::stop()
{
  if( this->isRunning() )
    {
      _stop = true;
      //this->exit(); // Stop this thread's event loop
      this->wait(); // Wait until this thread (i.e. function run()) exits
    }
}

// ----------------------------------------------------------------------------

#define USE_NATIVE_SOCKET

void ReceiverThread::run()
{
#ifndef USE_NATIVE_SOCKET
  // Use Qt socket
  _sock = new QUdpSocket();
  if( !_sock->bind( QHostAddress(_addr), _port ) )
    {
      _errString = QString("Failed to bind to adapter/port ") + _addrStr +
	QString(": ") + _sock->errorString();
      _stop = true;
    }
  // ### Cannot get this to work in combination with exec() below?
  //     (and with a QCoreApplication in SpidrDaq)
  //connect( _sock, SIGNAL( readyRead() ), this, SLOT( readDatagrams() ) );

#else
  // Use native socket, 'wrapped' in a QAbstractSocket object
  _sock = new QAbstractSocket( QAbstractSocket::UdpSocket, 0 );
  SOCKET sk;
  _stop = true;
#ifdef WIN32
  // Start up Winsock
  WSADATA wsadata;
  int err = WSAStartup( 0x0202, &wsadata );
  if( err != 0 )
    {
      _errString = (QString( "WSAStartup failed, error code " ) +
		    QString::number( err ));
    }
  else if( wsadata.wVersion != 0x0202 )
    {
      _errString = (QString( "Winsock version: " ) +
		    QString::number( wsadata.wVersion, 16 ));
    }
  else
#endif // WIN32
    {
      // Open a UDP socket
      sk = socket( AF_INET, SOCK_DGRAM, 0 );

      if( sk != INVALID_SOCKET )
	{
	  // Set socket option(s)
	  bool err_opt = false;

	  // Receive buffer size
	  int rcvbufsz = MPX_PIXELS * 64; // Will that be enough?
	  if( setsockopt( sk, SOL_SOCKET, SO_RCVBUF,
			  reinterpret_cast<char *> ( &rcvbufsz ),
			  sizeof( int ) ) != 0 )
	    {
	      _errString = QString("Failed to set socket option SO_RCVBUF");
	      err_opt = true;
	    }
	  /*
	  // Inspect the receive time-out setting..
#ifdef WIN32
	  int to;
	  typedef int socklen_t;
#else
	  struct timeval to;
#endif // WIN32
	  socklen_t len;
	  getsockopt( sk, SOL_SOCKET, SO_RCVTIMEO,
		      reinterpret_cast<char *> ( &to ), &len );
	  */
	  if( !err_opt )
	    {
	      // Bind the socket
	      struct sockaddr_in saddr;
	      saddr.sin_family      = AF_INET;
	      saddr.sin_port        = htons( _port );
	      saddr.sin_addr.s_addr = htonl( _addr );
	      if( bind( sk, ( struct sockaddr * ) &saddr,
			sizeof( struct sockaddr_in ) ) == 0 )
		{
		  // Assign the native socket to the Qt socket object
		  if( _sock->setSocketDescriptor( sk ) )
		    _stop = false;
		  else
		    _errString = QString("Failed to set descriptor");
		}
	      else
		{
		  _errString = QString("Failed to bind");
		}
	    }
	}
      else
	{
	  _errString = QString("Failed to create socket");
	}
    }
#endif // USE_NATIVE_SOCKET

  while( !_stop )
    {
      if( _sock->waitForReadyRead( 100 ) )
	this->readDatagrams();
      else
	this->handleFrameTimeout();
    }

  _sock->close();
  delete _sock;
}

// ----------------------------------------------------------------------------

void ReceiverThread::readDatagrams()
{
  int recvd_sz, sequence_nr, sequence_nr_modulo, shutter_cnt;

#ifndef USE_NATIVE_SOCKET
  while( _sock->hasPendingDatagrams() )
    {
      recvd_sz = _sock->readDatagram( _recvBuffer, RECV_BUF_SIZE );
      if( recvd_sz <= 0 ) continue;
#else
    {
      recvd_sz = _sock->read( _recvBuffer, RECV_BUF_SIZE );
      if( recvd_sz <= 0 ) return;
#endif // USE_NATIVE_SOCKET

      // Process the received packet
      ++_packetsReceived;
      sequence_nr = byteswap( _spidrHeader->sequenceNr ) - 1; // NB: minus 1..
      shutter_cnt = byteswap( _spidrHeader->shutterCnt );

      // Initialize shutter counter if necessary and determine
      // the expected number of packets per frame
      // (redo this at every new frame start, see below)
      if( _currShutterCnt == -1 )
	{
	  _currShutterCnt = shutter_cnt;

	  // Determine the used payload size
	  _expPayloadSize = recvd_sz - SPIDR_HEADER_SIZE;

	  // ..and from that the expected number of packets per frame
	  // (including a last packet that may contain less data)
	  _expPacketsPerFrame = ((_expFrameSize + (_expPayloadSize-1)) /
				 _expPayloadSize);

	  // Need to initialize the first loss count(down)!
	  // (subsequent counters are initialized in nextFrameBuffer())
	  _packetsLostFrame[_head] = _expPacketsPerFrame;
	}

      // For the 'two counters' readout the sequence number continues to
      // increase for the second frame containing the data from the 'high'
      // counter (added 21 Sep 2015)
      sequence_nr_modulo = sequence_nr % _expPacketsPerFrame;

      // Start of a new frame?
      // (NB: it was noticed (Wireshark) that with smaller packetsizes
      //      (ca.<1800 bytes) the order of the first 16 or so packets of
      //      the first frame was not sequential... This is not understood!
      //      (8 Oct 2015)
      //      ==> must be a system issue when starting an application:
      //          doing twice the same code results only in the first one
      //          having an unordered packet sequence)
      if( shutter_cnt != _currShutterCnt ||
	  // Another frame with the same shutter counter as previously?
	  // (happens e.g. with auto-trigger with just 1 trigger per sequence):
	  sequence_nr_modulo < _expSequenceNr )
	{
	  _currShutterCnt = shutter_cnt;

	  if( _expSequenceNr != _expPacketsPerFrame )
	    {
	      // Starting a new frame/image, but prematurely apparently,
	      // since '_expSequenceNr != _expPacketsPerFrame' means
	      // the frame wasn't complete yet...
	      // (see further down where a frame properly completes)
	      ++_framesReceived;
	      this->nextFrameBuffer();
	    }

	  // For the 'two counters' readout the sequence number continues to
	  // increase for the next frame containing the data from the second
	  // counter (added 21 Sep 2015), so if the sequence number exceeds
	  // the expected number we assume here the second counter ('CounterH')
	  // is being read out (added 30 Sep 2015)
	  if( sequence_nr == _expPacketsPerFrame )
	    _isCounterhFrame[_head] = true;
	  // NB: now done in releaseFrame():
	  //else if( sequence_nr == 0 )
	  //_isCounterhFrame[_head] = false;

	  // Any final packets lost in the previous sequence
	  // or any lost packets at the start of this new sequence ?
	  _packetsLost += (_expPacketsPerFrame - _expSequenceNr +
			   sequence_nr_modulo);

	  // The packet size may have changed (added 30 Sep 2015):
	  if( _expPayloadSize != recvd_sz - SPIDR_HEADER_SIZE )
	    {
	      // Determine the new payload size
	      _expPayloadSize = recvd_sz - SPIDR_HEADER_SIZE;

	      // ..and from that the expected number of packets per frame
	      // (including a last packet that may contain less data)
	      _expPacketsPerFrame = ((_expFrameSize + (_expPayloadSize-1)) /
				     _expPayloadSize);

	      // (Re)initialize the number of lost packets for this frame
	      _packetsLostFrame[_head] = _expPacketsPerFrame;
	    }
	}
      else if( sequence_nr_modulo > _expSequenceNr )
	{
	  // One or more packets lost in the ongoing frame sequence
	  _packetsLost += sequence_nr_modulo - _expSequenceNr;
	}

      // Next expected packet sequence number (NB: minus 1 compared to SPIDR)
      _expSequenceNr = sequence_nr_modulo + 1;

      // Copy the packet's payload to its expected location in the frame buffer
      // (but only when the sequence number is valid..)
      if( sequence_nr_modulo < _expPacketsPerFrame )
	{
	  memcpy( &_currFrameBuffer[sequence_nr_modulo * _expPayloadSize],
		  &_recvBuffer[SPIDR_HEADER_SIZE],
		  recvd_sz - SPIDR_HEADER_SIZE );
	  --_packetsLostFrame[_head]; // Is a countdown counter...
	}

      if( _copySpidrHeader )
	{
	  // Copy the SPIDR header
	  memcpy( _headerBuffer[_head], _recvBuffer, SPIDR_HEADER_SIZE );
	  _copySpidrHeader = false;
	}

      if( _expSequenceNr == _expPacketsPerFrame )
	{
	  // The current frame is now complete so go for a new frame/image
	  ++_framesReceived;
	  this->nextFrameBuffer();
	}
    }
}

// ----------------------------------------------------------------------------

void ReceiverThread::handleFrameTimeout()
{
  // Handle the case where the last packet(s) of the last frame
  // (since a while) has/have been lost, by means of a time-out (defined by
  // a multiple of the time-out in _sock->waitForReadyRead() above)
  if( _currShutterCnt >= 0 && _expSequenceNr != _expPacketsPerFrame )
    {
      ++_recvTimeoutCount;
      if( _recvTimeoutCount == 2 )
	{
	  // Most probably we lost the last packet(s) for this frame,
	  // declare the frame complete, count the loss and continue...
	  ++_framesReceived;
	  this->nextFrameBuffer();

	  _expSequenceNr = _expPacketsPerFrame; // Don't count losses again
	  _recvTimeoutCount = 0;
	}
    }
  else
    {
      _recvTimeoutCount = 0;
    }
}

// ----------------------------------------------------------------------------

void ReceiverThread::nextFrameBuffer()
{
  // Time-stamp the completion of a frame
  // (only done by the receiver that has and notifies a 'framebuilder')
  if( _frameBuilder )
    _timeStamp[_head] = QDateTime::currentDateTime();

  // Update the frame buffer management
  _mutex.lock();
  _head = (_head + 1) & (NR_OF_FRAMEBUFS-1);
  _empty = false;
  if( _head == _tail )
    {
      // Oops, no more buffers: going to overwrite the last frame received...
      ++_framesLost;
      if( _head == 0 )
	_head = NR_OF_FRAMEBUFS - 1;
      else
	--_head;
    }
  _mutex.unlock();

  if( _frameBuilder )
    {
      // Notify the framebuilder new data is available
      _frameBuilder->inputNotification();

      // Reset time-stamp to an invalid datetime for the next frame
      _timeStamp[_head] = QDateTime();
    }

  // Set pointer to the now active frame buffer
  _currFrameBuffer = _frameBuffer[_head];

  // Initialize the data in this frame buffer
  // (to be able to distinguish lost packets/data)
  memset( _currFrameBuffer, 0xFF, _expFrameSize );

  // Get the SPIDR's header from the first packet arriving
  _copySpidrHeader = true;

  // Initialize the number of lost packets for this frame
  _packetsLostFrame[_head] = _expPacketsPerFrame;
}

// ----------------------------------------------------------------------------

void ReceiverThread::releaseFrame()
{
  if( _empty ) return; // Safe-guard
  // Release the next frame buffer processed by the consumer:
  // update the frame buffer management
  _mutex.lock();
  _isCounterhFrame[_tail] = false;
  _tail = (_tail + 1) & (NR_OF_FRAMEBUFS-1);
  if( _tail == _head ) _empty = true;
  _mutex.unlock();
}

// ----------------------------------------------------------------------------

bool ReceiverThread::hasFrame()
{
  // NB: if this function is inlined it's no longer working properly
  //     e.g. in a while-loop probably due to optimization by the compiler...
  return( !_empty );
}

// ----------------------------------------------------------------------------

i64 ReceiverThread::timeStampFrame()
{
  // Return the time in (milli)seconds since 1970-01-01T00:00:00,
  // Coordinated Universal Time
#if QT_VERSION >= 0x040700
  if( _timeStamp[_tail].isValid() )
    return _timeStamp[_tail].toMSecsSinceEpoch();
  else
    return 0;
#else
  return _timeStamp[_tail].toTime_t();
#endif
}

// ----------------------------------------------------------------------------

i64 ReceiverThread::timeStampFrame( int i )
{
  // Return the time in (milli)seconds since 1970-01-01T00:00:00,
  // Coordinated Universal Time
#if QT_VERSION >= 0x040700
  if( _timeStamp[i].isValid() )
    return _timeStamp[i].toMSecsSinceEpoch();
  else
    return 0;
#else
  return _timeStamp[i].toTime_t();
#endif
}

// ----------------------------------------------------------------------------

i64 ReceiverThread::timeStampFrameSpidr()
{
  SpidrHeader_t *spidrhdr = (SpidrHeader_t *) _headerBuffer[_tail];
  i64 ts = (((i64) spidrhdr->timeLo) |
	    (((i64) spidrhdr->timeMi) << 16) |
	    (((i64) spidrhdr->timeHi) << 32));
  return ts;
}

// ----------------------------------------------------------------------------

void ReceiverThread::setPixelDepth( int nbits )
{
  // Determine the expected frame size in bytes
  // according to the selected pixel counter depth
  if( nbits == 1  ||
      nbits == 4  || // MPX3 only
      nbits == 6  || // MPX3-RX only
      nbits == 12 ||
      nbits == 24 )
    {
      if( nbits == 24 )
	nbits = 12; // A 24-bits frame is composed of two 12-bits frames
      _expFrameSize = (MPX_PIXELS * nbits) / 8;
      _pixelDepth = nbits;
    }
  else
    {
      // Illegal number of bits, set frame size to some value
      _expFrameSize = MPX3_12BIT_RAW_SIZE;
      _pixelDepth = 12;
    }
}

// ----------------------------------------------------------------------------

std::string ReceiverThread::ipAddressString()
{
  QString qs = _addrStr + ':' + QString::number( _port );
  return qs.toStdString();
}

// ----------------------------------------------------------------------------

std::string ReceiverThread::errString()
{
  if( _errString.isEmpty() ) return std::string( "" );
  QString qs = "Port " + QString::number( _port ) + ": " + _errString;
  return qs.toStdString();
}

// ----------------------------------------------------------------------------
