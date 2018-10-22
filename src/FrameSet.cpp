#include <cassert>
#include "FrameSet.h"

FrameSet::FrameSet()
{
    memset(frame, 0, sizeof (frame));
}

FrameSet::~FrameSet() {
    clear();
}

void FrameSet::clear() {
    for (int i = 0; i < number_of_chips; i++)
      for (int j = 0; j < 2; j++)
        if (frame[j][i] != nullptr) {
            //delete frame[j][i];
            //frame[j][i] = nullptr;
            frame[j][i]->clear();
        }
    counters = 1;
}

bool FrameSet::isComplete() {
    for (int j = 0; j < counters; j++)
        for (int i = 0; i < number_of_chips; i++)
            if (frame[j][i] == nullptr) return false;

    return true;
}

ChipFrame* FrameSet::takeChipFrame(int chipIndex, bool counterH) {
    assert (chipIndex >= 0 && chipIndex < number_of_chips);
    assert (! counterH || counters == 2);
    ChipFrame **spot = &(frame[counterH ? 1 : 0][chipIndex]);
    ChipFrame *result = *spot;
    if (result != nullptr) {
        *spot = nullptr;
    }
    return result;
}

void FrameSet::putChipFrame(int chipIndex, ChipFrame *cf) {
    assert (chipIndex >= 0 && chipIndex < number_of_chips);
    assert (cf != nullptr);
    // in 24 bit mode use both counters, not in CRW;
    // FOR NOW not in "both counters" mode!
    if (cf->omr.getCountL() == 3) counters = 2;
    int hi = (counters == 2 && cf->omr.getMode() == 4) ? 1 : 0;
    assert (hi == 0 || counters == 2);
    ChipFrame **spot = &(frame[hi][chipIndex]);
    if (*spot != nullptr) delete *spot;
    *spot = cf;
}

void FrameSet::copyTo32(int chipIndex, uint32_t *dest) {
    ChipFrame *f0 = frame[0][chipIndex];
    ChipFrame *f1 = frame[1][chipIndex];
    uint16_t * src0 = f0->getRow(0);
    int n = MPX_PIXELS;
    if (f1 == nullptr) {
        while (n--) *(dest++) = *(src0++);
    } else {
        uint16_t *src1 = f1->getRow(0);
        while (n--) *(dest++) = (uint32_t(*(src1++)) << 12) | *(src0++) ;
    }
}

void FrameSet::copyTo32(uint32_t *dest) {
    for (int i = 0; i < number_of_chips; i++) {
        copyTo32(i, dest);
        dest += MPX_PIXELS;
    }
}

int FrameSet::pixelsLost() {
    int count = 0;
    for (int j = 0; j < counters; j++)
        for (int i = 0; i < number_of_chips; i++)
            if (frame[j][i] == nullptr)
                count += MPX_PIXELS;
            else
                count += frame[j][i]->pixelsLost;

    return count;
}

