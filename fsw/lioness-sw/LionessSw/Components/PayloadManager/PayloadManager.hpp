// PayloadManager.hpp
// PayloadManager component implementation class. See
// PayloadManager.fpp for the design.
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef Components_PayloadManager_HPP
#define Components_PayloadManager_HPP

#include <atomic>
#include <cstdint>
#include <string>

#include "LionessSw/Components/PayloadManager/PayloadManagerComponentAc.hpp"
#include "LionessSw/Components/PayloadManager/StorageStub.hpp"

namespace Components {

class PayloadManager final : public PayloadManagerComponentBase {
public:
    PayloadManager(const char* const compName);
    ~PayloadManager();

    // Storage directory for the stub. Called from
    // FlatSatTopology.cpp. Default is "camera_storage" in the
    // current working directory.
    void setStorageDir(const std::string& dir) { storage_.setDirectory(dir); }
    const std::string& getStorageDir() const { return storage_.getDirectory(); }
    StorageStub& storage() { return storage_; }

    std::uint32_t getPicturesTaken() const { return picturesTaken_.load(); }
    std::uint32_t getPicturesFailed() const { return picturesFailed_.load(); }

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
    void TakePayloadPicture_cmdHandler(FwOpcodeType opCode, U32 cmdSeq,
                                       U32 width, U32 height,
                                       LionessSw::CaptureOperation operation) override;

    std::atomic<std::uint32_t> nextRequestId_;
    std::atomic<std::uint32_t> pendingRequestId_;
    std::atomic<std::uint32_t> picturesTaken_;
    std::atomic<std::uint32_t> picturesFailed_;
    std::uint64_t bytesStored_;
    StorageStub storage_;
};

}  // namespace Components

#endif
