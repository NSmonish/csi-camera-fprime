// ======================================================================
// \title  StarTracker.cpp
// \author kevinyang
// \brief  cpp file for StarTracker component implementation class
// ======================================================================

#include "LionessSw/Components/StarTracker/StarTracker.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

StarTracker ::StarTracker(const char* const compName)
    : StarTrackerComponentBase(compName), framesReceived_(0), lastFrameCounter_(0) {}

StarTracker ::~StarTracker() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void StarTracker::processFrame_handler(FwIndexType portNum,
                                       const Fw::Buffer& frame,
                                       U32 width, U32 height, U32 frameCounter) {
    (void)portNum;
    // A RAW10 packed frame has width * height * 10 / 8 bytes.
    // Reject anything that does not match, so a corrupt handoff
    // never reaches the star identification code.
    const U32 expectedBytes = width * height * 10 / 8;
    if (frame.getData() == nullptr || frame.getSize() != expectedBytes) {
        this->log_WARNING_HI_FrameInvalid(frameCounter, frame.getSize());
        return;
    }
    ++framesReceived_;
    lastFrameCounter_.store(frameCounter);
    this->tlmWrite_FramesReceived(framesReceived_.load());
    this->tlmWrite_LastFrameCounter(lastFrameCounter_.load());
    this->log_ACTIVITY_HI_FrameReceived(frameCounter, width, height);
}

}  // namespace Components
