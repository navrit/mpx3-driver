#ifndef FRAMESETMANAGER_H
#define FRAMESETMANAGER_H

#include "FrameSet.h"

class FrameSetManager
{
public:
    FrameSetManager();

    void clearFrame(uint8_t frameId);
    void putChipFrame(uint8_t frameId, int chipIndex, ChipFrame* cf);
    ChipFrame *newChipFrame();

private:
    FrameSet fs[256];
};

#endif // FRAMESETMANAGER_H
