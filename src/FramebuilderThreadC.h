#ifndef FRAMEBUILDERTHREADC_H
#define FRAMEBUILDERTHREADC_H

#include "FramebuilderThread.h"

class FramebuilderThreadC : public FramebuilderThread
{
  Q_OBJECT

 public:
  FramebuilderThreadC( std::vector<ReceiverThread *> recvrs,
                    QObject *parent = nullptr );
  virtual ~FramebuilderThreadC();

  virtual void processFrame();
  virtual int  mpx3RawToPixel( unsigned char *raw_bytes,
                               int            nbytes,
                               int            chipIndex);
};

#endif // FRAMEBUILDERTHREADC_H
