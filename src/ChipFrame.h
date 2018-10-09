#ifndef CHIPFRAME_H
#define CHIPFRAME_H

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
    short * getRow(int rowNum) { return data + 256 * rowNum; }
    void finish();
private:
    short data[MPX_PIXELS];
};

#endif // CHIPFRAME_H
