#ifndef RECEIVERTHREADC_H
#define RECEIVERTHREADC_H

#include "ReceiverThread.h"

const u64 PKT_TYPE_MASK     = 0xF000000000000000;
const u64 INFO_HEADER_SOF   = 0x9000000000000000;
const u64 INFO_HEADER_MID   = 0x1000000000000000;
//const u64 INFO_HEADER_EOF = 0x5000000000000000; // Before 12 Feb 2016
const u64 INFO_HEADER_EOF   = 0xD000000000000000;
const u64 PIXEL_DATA_SOR    = 0xA000000000000000;
//const u64 PIXEL_DATA_EOR  = 0x6000000000000000; // Before 12 Feb 2016
const u64 PIXEL_DATA_EOR    = 0xE000000000000000;
const u64 PIXEL_DATA_SOF    = 0xB000000000000000;
const u64 PIXEL_DATA_EOF    = 0x7000000000000000;
const u64 PIXEL_DATA_MID    = 0x3000000000000000;

const u64 ROW_COUNT_MASK    = 0x0FF0000000000000;
const u64 FRAME_FLAGS_MASK  = 0x000FFFF000000000;
const u64 ROW_COUNT_SHIFT   = 52;
const u64 FRAME_FLAGS_SHIFT = 36;

class ReceiverThreadC : public ReceiverThread
{
  Q_OBJECT

 public:
  ReceiverThreadC( int     *ipaddr,
		  int      port = 8192,
		  QObject *parent = 0 );
  virtual ~ReceiverThreadC();
  
  virtual void readDatagrams();
  void         nextFrame();
  virtual int  dataSizeFrame()          { return _frameSize[_tail]; }

  virtual int  pixelsReceived()         { return _pixelsReceived; }
  virtual int  pixelsLost()             { return _pixelsLost; }
  virtual int  pixelsLostFrame()        { return _pixelsLostFrame[_tail]; }
  virtual int  pixelsLostFrame( int i ) { return _pixelsLostFrame[i]; }

  virtual int  lostCount()              { return pixelsLost(); }
  virtual int  lostCountFrame()         { return pixelsLostFrame(); }
  virtual void resetLost()              { ReceiverThread::resetLost();
                                          _pixelsLost = 0; }

 private:
  // Expected frame data
  int     _rowCnt, _rowPixels;
  u64    *_framePtr;
  int     _frameSize[NR_OF_FRAMEBUFS];
  u16     _shutterCnt;
  bool    _bigEndian;
  char    _infoHeader[256/8]; // Storage for a single info header
  int     _infoIndex;

  // Statistics
  int     _pixelsReceived;
  int     _pixelsLost;
  int     _pixelsLostFrame[NR_OF_FRAMEBUFS];
};

#endif // RECEIVERTHREADC_H
