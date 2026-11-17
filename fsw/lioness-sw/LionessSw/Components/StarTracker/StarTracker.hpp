// ======================================================================
// \title  StarTracker.hpp
// \author kevinyang
// \brief  hpp file for StarTracker component implementation class
// ======================================================================

#ifndef Components_StarTracker_HPP
#define Components_StarTracker_HPP

#include <atomic>

#include "LionessSw/Components/StarTracker/StarTrackerComponentAc.hpp"

namespace Components {

class StarTracker final : public StarTrackerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct StarTracker object
    StarTracker(const char* const compName  //!< The component name
    );

    //! Destroy StarTracker object
    ~StarTracker();

    //! Frames received, for tests
    U32 getFramesReceived() const { return framesReceived_.load(); }
    //! Last frame counter received, for tests
    U32 getLastFrameCounter() const { return lastFrameCounter_.load(); }

  private:
    // Handler for the processFrame input port. Signature must
    // match the generated base class exactly.
    void processFrame_handler(FwIndexType portNum, const Fw::Buffer& frame,
                              U32 width, U32 height,
                              U32 frameCounter) override;
    // Atomic: processFrame runs on the caller's thread
    // while the test reads these from its own thread.
    std::atomic<U32> framesReceived_;
    std::atomic<U32> lastFrameCounter_;
};

}  // namespace Components

#endif
