// StarTrackerManager.cpp
// See StarTrackerManager.hpp.
// Style rule for this project: plain punctuation only, no em dashes.

#include "LionessSw/Components/StarTrackerManager/StarTrackerManager.hpp"

namespace Components {

StarTrackerManager::StarTrackerManager(const char* const compName)
    : StarTrackerManagerComponentBase(compName), nextRequestId_(1),
      pendingRequestId_(0), starFramesTaken_(0), starFramesFailed_(0) {}

StarTrackerManager::~StarTrackerManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void StarTrackerManager::captureDone_handler(
    FwIndexType portNum, std::uint32_t requestId,
    const LionessSw::CaptureStatus& status,
    const LionessSw::CameraId& cameraId, std::uint32_t width,
    std::uint32_t height, std::uint32_t frameBytes, std::uint32_t frameCounter,
    std::uint32_t previewBytes, std::uint32_t losslessBytes,
    std::uint32_t pngBytes, std::uint64_t sourceChecksum,
    const Fw::Buffer& preview, const Fw::Buffer& lossless,
    const Fw::Buffer& png, const Fw::Buffer& source) {
    (void)portNum;
    (void)cameraId;
    (void)frameBytes;
    (void)previewBytes;
    (void)losslessBytes;
    (void)pngBytes;
    (void)sourceChecksum;
    (void)preview;
    (void)lossless;
    (void)png;
    pendingRequestId_.store(0);
    if (status != LionessSw::CaptureStatus::OK) {
        std::uint32_t failed = ++starFramesFailed_;
        this->tlmWrite_StarFramesFailed(failed);
        this->log_WARNING_HI_StarFrameFailed(requestId, status);
        return;
    }
    // Hand the raw frame to StarTracker, then return the buffer
    // to the Camera component.
    if (this->isConnected_processFrame_OutputPort(0)) {
        this->processFrame_out(0, source, width, height, frameCounter);
    }
    if (this->isConnected_returnFrame_OutputPort(0)) {
        this->returnFrame_out(0, source);
    }
    std::uint32_t taken = ++starFramesTaken_;
    this->tlmWrite_StarFramesTaken(taken);
    this->log_ACTIVITY_HI_StarFrameDelivered(requestId, frameCounter);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void StarTrackerManager::TakeStarPicture_cmdHandler(FwOpcodeType opCode,
                                                    U32 cmdSeq, U32 width,
                                                    U32 height) {
    std::uint32_t requestId = nextRequestId_.fetch_add(1);
    if (!this->isConnected_captureSubmit_OutputPort(0)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    LionessSw::CaptureStatus status = this->captureSubmit_out(
        0, requestId, LionessSw::CameraId::STAR_TRACKER, width, height,
        LionessSw::CaptureOperation::RAW, 500);
    if (status != LionessSw::CaptureStatus::OK) {
        if (status == LionessSw::CaptureStatus::BUSY) {
            this->log_WARNING_LO_CameraBusy(requestId);
        } else {
            this->log_WARNING_LO_CameraInvalid(requestId);
        }
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    pendingRequestId_.store(requestId);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
