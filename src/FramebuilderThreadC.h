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
                               int           *pixels,
                               int            counter_depth,
                               //int          device_type,
                               bool           is_counterh ); //compress );
};

#endif // FRAMEBUILDERTHREADC_H
