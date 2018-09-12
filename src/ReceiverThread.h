#ifndef RECEIVERTHREAD_H
#define RECEIVERTHREAD_H

#include <QDateTime>
#include <QMutex>
#include <QString>
#include <QThread>

#ifdef WIN32
  #include "stdint.h"
#else
  #include </usr/include/stdint.h>
#endif
typedef int64_t  i64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
#include "spidrdata.h"

#define NR_OF_FRAMEBUFS  16 // Use a power of 2
#define FRAMEBUF_SIZE    MPX3_MAX_FRAME_SIZE
#define RECV_BUF_SIZE    16384

class QUdpSocket;
class QAbstractSocket;
class FramebuilderThread;
class SpidrController;

class ReceiverThread : public QThread
{
  Q_OBJECT

 public:
  ReceiverThread( int     *ipaddr,
		  int      port = 8192,
		  QObject *parent = 0 );
  virtual ~ReceiverThread();
  
  void stop();

  void run();

  virtual void readDatagrams();
  void handleFrameTimeout();
  void nextFrameBuffer();
  void releaseFrame();
  bool hasFrame();
  i64  timeStampFrame();
  i64  timeStampFrameSpidr();
  virtual int dataSizeFrame() { return _expFrameSize; }
  u8  *frameData()        { return _frameBuffer[_tail]; } 
  u8  *spidrHeaderFrame() { return _headerBuffer[_tail]; }
  bool isCounterhFrame()  { return _isCounterhFrame[_tail]; }
  int  packetsLostFrame() { return _packetsLostFrame[_tail]; }
  int  packetsLostFrame( int i ) { return _packetsLostFrame[i]; }
  i64  timeStampFrame( int i );
  virtual int frameFlags() { return 0; }

  void setPixelDepth( int nbits );

  std::string ipAddressString();
  std::string errString();
  void clearErrString()   { _errString.clear(); };

  int  framesReceived()   { return _framesReceived; }
  int  framesLost()       { return _framesLost; }
  int  packetsReceived()  { return _packetsReceived; }
  int  packetsLost()      { return _packetsLost; }
  virtual void resetLost(){ _framesLost = 0; _packetsLost = 0; }
  int  packetSize()       { return _expPayloadSize + SPIDR_HEADER_SIZE; }
  int  expSequenceNr()    { return _expSequenceNr; }

  virtual int pixelsReceived()         { return 0; }
  virtual int pixelsLost()             { return 0; }
  virtual int pixelsLostFrame()        { return 0; }
  virtual int pixelsLostFrame( int i ) { i=0; return 0; }

  virtual int lostCount()              { return packetsLost(); }
  virtual int lostCountFrame()         { return packetsLostFrame(); }
  virtual int lostCountFrame( int i )  { return packetsLostFrame(i); }

  void setFramebuilder( FramebuilderThread *framebuilder )
    { _frameBuilder = framebuilder; };
  void setController( SpidrController *spidrctrl )
    { _spidrController = spidrctrl; };

 protected:
#define USE_NATIVE_SOCKET
#ifdef USE_NATIVE_SOCKET
  QAbstractSocket *_sock;
#else
  QUdpSocket *_sock;
#endif
  quint32     _addr;
  QString     _addrStr;
  int         _port;
  bool        _stop;
  FramebuilderThread *_frameBuilder;
  SpidrController    *_spidrController;

  // Expected frame data
  int     _expFrameSize;
  int     _expPayloadSize;
  int     _expPacketsPerFrame;
  int     _currShutterCnt;
  int     _expSequenceNr;
  bool    _copySpidrHeader;
  int     _pixelDepth;

  // Statistics
  int     _framesReceived;
  int     _framesLost;
  int     _packetsReceived;
  int     _packetsLost;
  int     _packetsLostFrame[NR_OF_FRAMEBUFS];

  // String containing a description of the last error that occurred
  QString _errString;

  // Frame buffers administration
  QMutex  _mutex;
  int     _head, _tail;
  bool    _empty;
  u8     *_currFrameBuffer;
  int     _recvTimeoutCount;

  // For receive buffer
  SpidrHeader_t *_spidrHeader;

  // Receive buffer for a single datagram/IP-packet
  char      _recvBuffer[RECV_BUF_SIZE];

  // Frame info and buffers
  QDateTime _timeStamp[NR_OF_FRAMEBUFS];
  bool      _isCounterhFrame[NR_OF_FRAMEBUFS];
  u8        _headerBuffer[NR_OF_FRAMEBUFS][SPIDR_HEADER_SIZE];
  u8        _frameBuffer[NR_OF_FRAMEBUFS][FRAMEBUF_SIZE];
};

#endif // RECEIVERTHREAD_H
