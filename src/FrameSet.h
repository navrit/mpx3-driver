#ifndef FRAMESET_H
#define FRAMESET_H

#include "configs.h"
#include "ChipFrame.h"

class FrameSet
{
public:
    FrameSet();
    void clear();
    void putChipFrame(int chipIndex, ChipFrame * cf);
    bool isComplete();
    void copyTo32(uint32_t *dest);

private:
    ChipFrame* frame[2][Config::number_of_chips];

};

#endif // FRAMESET_H
