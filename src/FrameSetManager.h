#ifndef FRAMESETMANAGER_H
#define FRAMESETMANAGER_H

#include <mutex>
#include "FrameSet.h"

class FrameSetManager
{
public:
    FrameSetManager();

    void clearFrame(uint8_t frameId);
    void putChipFrame(uint8_t frameId, int chipIndex, ChipFrame* cf);
    ChipFrame *newChipFrame();
    bool isFull();
    bool isEmpty();
    FrameSet *getFrameSet();
    void releaseFrameSet();

private:
    FrameSet fs[256];
    int dropped = 0;
    uint8_t head_ = 0, tail_ = 0, ahead_ = 0;
    uint16_t with_client = 50000;
    std::mutex mutex_;
};

#endif // FRAMESETMANAGER_H
