#ifndef CHIPFRAME_H
#define CHIPFRAME_H

#include <stdint.h>
#include <cstring>
#include "mpx3defs.h"
#include "OMR.h"

class ChipFrame
{
public:
    ChipFrame();
    OMR omr;
    int brokenRows;
    void clear() { memset(data, 0, sizeof(data));}
    //bool isEmpty();
    uint16_t * getRow(int rowNum) { return data + MPX_PIXEL_COLUMNS * rowNum; }
    void finish();
private:
    uint16_t data[MPX_PIXELS];
};

#endif // CHIPFRAME_H
