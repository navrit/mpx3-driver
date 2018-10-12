#include "FrameSetManager.h"

FrameSetManager::FrameSetManager()
{

}

bool FrameSetManager::isFull() {
    std::lock_guard<std::mutex> lock(mutex_);
    return head_ + 1 == tail_;
}

bool FrameSetManager::isEmpty() {
    std::lock_guard<std::mutex> lock(mutex_);
    return head_ == tail_;
}

FrameSet * FrameSetManager::getFrameSet() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (tail_ != head_ && fs[tail_].isEmpty()) tail_++;
    if (head_ == tail_) return nullptr;
    with_client = tail_;
    return fs + tail_;
}

void FrameSetManager::releaseFrameSet() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tail_ == with_client)
        clearFrame(tail_++);
    with_client = 50000;
}

void FrameSetManager::clearFrame(uint8_t frameId) {
    fs[frameId].clear();
    // later: put the ChipFrame back in the pool
}

void FrameSetManager::putChipFrame(uint8_t frameId, int chipIndex, ChipFrame* cf) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (head_ == frameId) {
        // that's normally the case
    } else if (head_ == tail_) {
        // new frame in an empty buffer, except there could be some under construction
        while (tail_ != frameId && fs[tail_].isEmpty()) { head_ = ++tail_; }
        while (head_ != frameId && ! fs[head_].isComplete()) head_++;
    }
    fs[frameId].putChipFrame(chipIndex, cf);
    if (fs[frameId].isComplete()) head_ = frameId + 1;
}


ChipFrame *FrameSetManager::newChipFrame() {
    return new ChipFrame();
}
