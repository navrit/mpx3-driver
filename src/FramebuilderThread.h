#ifndef FRAMEBUILDERTHREAD_H
#define FRAMEBUILDERTHREAD_H

#include <QFile>
#include <QMutex>
#include <QString>
#include <QThread>
#include <QWaitCondition>

#include <vector>

#ifdef WIN32
  #include "stdint.h"
#else
  #include </usr/include/stdint.h>
#endif
typedef int64_t  i64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
#include "spidrdata.h"
#include "FrameSet.h"
#include "FrameSetManager.h"

#define NR_OF_DEVICES  4

class ReceiverThread;

typedef void (*CallbackFunc)( int id );

class FramebuilderThread : public QThread
{
  Q_OBJECT

 public:
  FramebuilderThread( std::vector<ReceiverThread *> recvrs,
                    QObject *parent = nullptr );
  virtual ~FramebuilderThread();

  void   stop();
  void   run();
  void   inputNotification();
  void   abortFrame();
  virtual void processFrame();
  bool   hasFrame( unsigned long timeout_ms = 0 );
  FrameSet   *frameData();
  void   clearFrameData( int index );
  void   releaseFrame(FrameSet*fs);
  i64    frameTimestamp();
  double frameTimestampDouble();
  i64    frameTimestampSpidr();
  int    frameShutterCounter( int index = -1 );

  void   setAddrInfo( int *ipaddr, int *ports );
  void   setDeviceIdsAndTypes( int *ids, int *types );
  void   setPixelDepth( int nbits );
  void   setDecodeFrames( bool decode );
  void   setCompressFrames( bool compress );
  void   setLutEnable( bool enable ) { _applyLut = enable; }
  void   setLutBug( bool bug ) { _lutBug = bug; }
  void   setFlush( bool flush ) { _flush = flush; }

  void   setCallbackId( int id ) { _id = id; }
  void   setCallback( CallbackFunc cbf ) { _callbackFunc = cbf; }

  bool   openFile( std::string filename, bool overwrite = false );
  bool   closeFile();

  int    framesReceived() { return _framesReceived; }
  int    framesWritten()  { return _framesWritten; }
  int    framesProcessed(){ return _framesProcessed; }
  int    lostCount()      { return _lostCountTotal; }
  int    lostCountFrame();

  std::string errString();
  void clearErrString() { _errString.clear(); };

  FrameSetManager *pFrameSetManager;
 protected:
  // Vector with pointers to frame receivers (up to 4 of them)
  std::vector<ReceiverThread *> _receivers;
  u32 _n; // To contain size of _receivers

  QMutex         _mutex;
  QWaitCondition _inputCondition;
  QWaitCondition _outputCondition;
  bool           _stop;

  // For external notification purposes, a general-purpose callback function
  // with 1 parameter (initially for Pixelman)
  int          _id;
  CallbackFunc _callbackFunc;

  // Event header buffer
  EvtHeader_t _evtHdr;
  // Device headers
  DevHeader_t _devHdr[NR_OF_DEVICES];

  int   _framesReceived;
  int   _framesWritten;
  int   _framesProcessed;
  int   _lostCountTotal;
  bool  _decode;
  bool  _compress;
  bool  _flush;
  bool  _abortFrame;

  QFile _file;
  bool  _fileOpen;

  // String containing a description of the last error that occurred
  QString _errString;

  // Look-up tables for Medipix3RX pixel data decoding
  int           _mpx3Rx6BitsLut[64];
  int           _mpx3Rx6BitsEnc[64];
  int           _mpx3Rx12BitsLut[4096];
  int           _mpx3Rx12BitsEnc[4096];
  bool          _applyLut;
  bool          _lutBug;

  // Info about the (decoded) set of frames
  i64           _timeStamp;
  i64           _timeStampSpidr;
  int           _frameId[NR_OF_DEVICES];
  SpidrHeader_t _spidrHeader[NR_OF_DEVICES];


  virtual int mpx3RawToPixel( unsigned char *raw_bytes,
                              int            nbytes,
                              int			chipIndex);
};

#endif // FILEWRITERTHREAD_H
