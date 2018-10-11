#include "FrameSetManager.h"

FrameSetManager::FrameSetManager()
{

}

void FrameSetManager::clearFrame(uint8_t frameId) {
    fs[frameId].clear();
    // later: put the ChipFrame back in the pool
}

void FrameSetManager::putChipFrame(uint8_t frameId, int chipIndex, ChipFrame* cf) {
    fs[frameId].putChipFrame(chipIndex, cf);
}


ChipFrame *FrameSetManager::newChipFrame() {
    return new ChipFrame();
}
