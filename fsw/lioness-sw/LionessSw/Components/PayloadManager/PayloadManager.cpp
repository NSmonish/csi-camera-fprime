// PayloadManager.cpp
// See PayloadManager.hpp.
// Style rule for this project: plain punctuation only, no em dashes.

#include "LionessSw/Components/PayloadManager/PayloadManager.hpp"

#include <cstdio>

namespace Components {

PayloadManager::PayloadManager(const char* const compName)
    : PayloadManagerComponentBase(compName), nextRequestId_(1),
      pendingRequestId_(0), picturesTaken_(0), picturesFailed_(0),
      bytesStored_(0), storage_("camera_storage") {}

PayloadManager::~PayloadManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PayloadManager::captureDone_handler(
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
    (void)width;
    (void)height;
    (void)frameBytes;
    (void)frameCounter;
    (void)sourceChecksum;
    (void)source;
    pendingRequestId_.store(0);
    if (status != LionessSw::CaptureStatus::OK) {
        std::uint32_t failed = ++picturesFailed_;
        this->tlmWrite_PicturesFailed(failed);
        this->log_WARNING_HI_PictureFailed(requestId, status);
        return;
    }
    // Store the three products: the JPEG preview, the PNG
    // preview of the same frame, and the zlib lossless product.
    // The PNG is a viewable preview, not the lossless product.
    bool stored = true;
    char previewName[64];
    (void)std::snprintf(previewName, sizeof(previewName),
                        "payload_%u_preview.jpg", requestId);
    char pngName[64];
    (void)std::snprintf(pngName, sizeof(pngName),
                        "payload_%u_preview.png", requestId);
    char losslessName[64];
    (void)std::snprintf(losslessName, sizeof(losslessName),
                        "payload_%u_lossless.zlib", requestId);
    if (preview.getSize() > 0) {
        if (!storage_.writeFile(previewName, preview.getData(),
                                preview.getSize())) {
            stored = false;
        }
    }
    if (stored && png.getSize() > 0) {
        if (!storage_.writeFile(pngName, png.getData(), png.getSize())) {
            stored = false;
        }
    }
    if (stored && lossless.getSize() > 0) {
        if (!storage_.writeFile(losslessName, lossless.getData(),
                                lossless.getSize())) {
            stored = false;
        }
    }
    // Return the processed buffers to ImageProcessing now that
    // the files are written.
    if (preview.getSize() > 0 &&
        this->isConnected_returnOutput_OutputPort(0)) {
        this->returnOutput_out(0, preview);
    }
    if (png.getSize() > 0 &&
        this->isConnected_returnOutput_OutputPort(0)) {
        this->returnOutput_out(0, png);
    }
    if (lossless.getSize() > 0 &&
        this->isConnected_returnOutput_OutputPort(0)) {
        this->returnOutput_out(0, lossless);
    }
    if (!stored) {
        std::uint32_t failed = ++picturesFailed_;
        this->tlmWrite_PicturesFailed(failed);
        this->log_WARNING_HI_StorageFailed(requestId);
        return;
    }
    bytesStored_ += previewBytes + pngBytes + losslessBytes;
    this->tlmWrite_BytesStored(bytesStored_);
    std::uint32_t taken = ++picturesTaken_;
    this->tlmWrite_PicturesTaken(taken);
    this->log_ACTIVITY_HI_PictureStored(requestId, previewBytes, losslessBytes,
                                        pngBytes);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PayloadManager::TakePayloadPicture_cmdHandler(
    FwOpcodeType opCode, U32 cmdSeq, U32 width, U32 height,
    LionessSw::CaptureOperation operation) {
    std::uint32_t requestId = nextRequestId_.fetch_add(1);
    if (!this->isConnected_captureSubmit_OutputPort(0)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    LionessSw::CaptureStatus status = this->captureSubmit_out(
        0, requestId, LionessSw::CameraId::PAYLOAD, width, height, operation,
        500);
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
