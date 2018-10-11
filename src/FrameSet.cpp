#include <cassert>
#include "FrameSet.h"

FrameSet::FrameSet()
{

}

void FrameSet::clear() {
    for (int i = 0; i < Config::number_of_chips; i++)
      for (int j = 0; j < 2; j++)
        if (frame[j][i] != nullptr) {
            delete frame[j][i];
            frame[j][i] = nullptr;
        }
}

bool FrameSet::isComplete() {
    for (int i = 0; i < Config::number_of_chips; i++)
        if (frame[0][i] == nullptr) return false;

    if (frame[0][0]->omr.getCountL() == 3) {
        for (int i = 0; i < Config::number_of_chips; i++)
            if (frame[1][i] == nullptr) return false;
    }
}

void FrameSet::putChipFrame(int chipIndex, ChipFrame *cf) {
    assert (chipIndex >= 0 && chipIndex < Config::number_of_chips);
    assert (cf != nullptr);
    frame[cf->omr.getMode() == 1 ? 1 : 0][chipIndex] = cf;
}

void FrameSet::copyTo32(uint32_t *dest) {
    for (int i = 0; i < Config::number_of_chips; i++) {
        ChipFrame *f0 = frame[0][i];
        ChipFrame *f1 = frame[1][i];
        uint16_t * src0 = f0->getRow(0);
        int n = MPX_PIXELS;
        if (f1 == nullptr) {
            while (n--) *(dest++) = *(src0++);
        } else {
            uint16_t *src1 = f1->getRow(0);
            while (n--) *(dest++) = (uint32_t(*(src1++)) << 12) || *(src0++) ;
        }
    }
}
