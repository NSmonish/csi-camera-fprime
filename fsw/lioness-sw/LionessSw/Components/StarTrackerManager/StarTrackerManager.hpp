// StarTrackerManager.hpp
// StarTrackerManager component implementation class. See
// StarTrackerManager.fpp for the design.
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef Components_StarTrackerManager_HPP
#define Components_StarTrackerManager_HPP

#include <atomic>
#include <cstdint>

#include "LionessSw/Components/StarTrackerManager/StarTrackerManagerComponentAc.hpp"

namespace Components {

class StarTrackerManager final : public StarTrackerManagerComponentBase {
public:
    StarTrackerManager(const char* const compName);
    ~StarTrackerManager();

    std::uint32_t getStarFramesTaken() const { return starFramesTaken_.load(); }
    std::uint32_t getStarFramesFailed() const { return starFramesFailed_.load(); }

private:
    // Handlers for the typed input port and the command. The
    // signatures must match the generated base class exactly.
    void captureDone_handler(
        FwIndexType portNum, std::uint32_t requestId,
        const LionessSw::CaptureStatus& status,
        const LionessSw::CameraId& cameraId, std::uint32_t width,
        std::uint32_t height, std::uint32_t frameBytes,
        std::uint32_t frameCounter, std::uint32_t previewBytes,
        std::uint32_t losslessBytes, std::uint32_t pngBytes,
        std::uint64_t sourceChecksum, const Fw::Buffer& preview,
        const Fw::Buffer& lossless, const Fw::Buffer& png,
        const Fw::Buffer& source) override;
    void TakeStarPicture_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 width,
                                    U32 height) override;

    std::atomic<std::uint32_t> nextRequestId_;
    std::atomic<std::uint32_t> pendingRequestId_;
    std::atomic<std::uint32_t> starFramesTaken_;
    std::atomic<std::uint32_t> starFramesFailed_;
};

}  // namespace Components

#endif
