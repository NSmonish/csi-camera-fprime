// Camera.hpp
// Camera component implementation class. Owns the two cameras
// through CameraHal::ICameraDevice, one for the payload camera
// and one for the star tracker camera. Each camera has its own
// job slot and its own worker thread, so a payload capture and
// a star capture can run at the same time. V4L2 queues buffers
// per device, it does not queue requests from two managers, so
// the per-device slots live here, not in the HAL. A short
// synchronous submit validates the request and reserves the
// job for that one device. A second submit for the same device
// while it is busy returns Busy. The worker thread polls the
// device with a timeout, then hands the frame to
// ImageProcessing or straight to the StarTrackerManager.
// Exactly one completion is sent per accepted request, and the
// device buffer is returned before the completion goes out.
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef Components_Camera_HPP
#define Components_Camera_HPP

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "LionessSw/Components/Camera/CameraComponentAc.hpp"
#include "LionessSw/V4L2/CameraDevice.hpp"

namespace Components {

class Camera final : public CameraComponentBase {
public:
    // Largest frame this component accepts: 4608x2592 RAW10.
    static const std::size_t kMaxFrameBytes = 14929920;
    static const std::uint32_t kRingBuffers = 4;

    Camera(const char* const compName);
    ~Camera();

    // Stop the worker thread and wait for it to finish. Used by
    // tests so no worker callback can run after the tester that
    // receives it is destroyed. Safe to call more than once.
    void shutdownWorker();

    // Inject the devices. Called from FlatSatTopology.cpp before
    // the tasks start. The Camera component does not own the
    // devices; they must outlive the component. Until a device
    // is set, submits for that camera return Invalid.
    void setPayloadDevice(CameraHal::ICameraDevice* device) {
        payloadDevice_ = device;
    }
    void setStarDevice(CameraHal::ICameraDevice* device) {
        starDevice_ = device;
    }
    CameraHal::ICameraDevice* getPayloadDevice() const { return payloadDevice_; }
    CameraHal::ICameraDevice* getStarDevice() const { return starDevice_; }

    // Device paths used when opening the devices, empty for
    // auto-detect (real driver) or ignored (simulated device).
    // Set from FlatSatTopology.cpp before the tasks start.
    void setPayloadDevicePath(const std::string& path) {
        payloadDevicePath_ = path;
    }
    void setStarDevicePath(const std::string& path) {
        starDevicePath_ = path;
    }

    // For tests: whether any job is outstanding, and whether
    // the job for one camera is outstanding.
    bool isBusy() const {
        return payloadSlot_.busy.load() || starSlot_.busy.load();
    }
    bool isBusy(LionessSw::CameraId cameraId) const {
        return slotFor(cameraId).busy.load();
    }

private:
    struct Job {
        std::uint32_t requestId;
        LionessSw::CameraId cameraId;
        std::uint32_t width;
        std::uint32_t height;
        LionessSw::CaptureOperation operation;
        std::uint32_t timeoutMs;
        std::uint32_t frameCounter;  // filled in by the worker
        std::uint32_t frameBytes;    // filled in by the worker

        Job()
            : requestId(0), cameraId(LionessSw::CameraId::PAYLOAD), width(0),
              height(0), operation(LionessSw::CaptureOperation::BOTH),
              timeoutMs(500), frameCounter(0), frameBytes(0) {}
    };

    // Handlers for the typed input ports and the command. The
    // signatures must match the generated base class exactly:
    // enums and Fw::Buffer arrive as const references on ports,
    // by value on commands.
    LionessSw::CaptureStatus captureSubmitPayload_handler(
        FwIndexType portNum, std::uint32_t requestId,
        const LionessSw::CameraId& cameraId, std::uint32_t width,
        std::uint32_t height, const LionessSw::CaptureOperation& operation,
        std::uint32_t timeoutMs) override;
    LionessSw::CaptureStatus captureSubmitStar_handler(
        FwIndexType portNum, std::uint32_t requestId,
        const LionessSw::CameraId& cameraId, std::uint32_t width,
        std::uint32_t height, const LionessSw::CaptureOperation& operation,
        std::uint32_t timeoutMs) override;
    void processDone_handler(
        FwIndexType portNum, std::uint32_t requestId,
        const LionessSw::CameraId& cameraId,
        const LionessSw::CaptureStatus& status, std::uint32_t width,
        std::uint32_t height, std::uint32_t previewBytes,
        std::uint32_t losslessBytes, std::uint32_t pngBytes,
        std::uint64_t sourceChecksum, const Fw::Buffer& preview,
        const Fw::Buffer& lossless, const Fw::Buffer& png) override;
    void returnFrame_handler(FwIndexType portNum,
                             const Fw::Buffer& buffer) override;
    void TakePicture_cmdHandler(FwOpcodeType opCode, U32 cmdSeq,
                                LionessSw::CameraId cameraId, U32 width,
                                U32 height,
                                LionessSw::CaptureOperation operation,
                                U32 timeoutMs) override;

    // Shared submit path for the two sync ports and the command.
    LionessSw::CaptureStatus submit(const Job& job);

    // Configure the device for this job if needed: stop the
    // stream, set the format, allocate ring buffers, start the
    // stream. Returns false on any failure.
    bool configureDevice(CameraHal::ICameraDevice* device,
                         const std::string& devicePath, std::uint32_t width,
                         std::uint32_t height, std::uint32_t& configuredWidth,
                         std::uint32_t& configuredHeight);

    // One job slot per camera. Each slot has its own busy flag,
    // pending job, pending ImageProcessing job, condition
    // variable and worker thread. The two workers share mutex_
    // and stop_, and each device has its own V4L2 buffer ring.
    struct Slot {
        std::atomic<bool> busy;
        bool hasPendingJob;
        Job pendingJob;
        Job pendingProcessJob;
        bool hasPendingProcessJob;
        CameraHal::ICameraDevice* pendingProcessDevice;
        std::uint32_t pendingProcessBufIndex;
        std::condition_variable hasJob;
        std::thread worker;

        Slot()
            : busy(false), hasPendingJob(false), hasPendingProcessJob(false),
              pendingProcessDevice(nullptr), pendingProcessBufIndex(0) {}
    };

    Slot& slotFor(LionessSw::CameraId cameraId) {
        return (cameraId == LionessSw::CameraId::PAYLOAD) ? payloadSlot_
                                                          : starSlot_;
    }
    const Slot& slotFor(LionessSw::CameraId cameraId) const {
        return (cameraId == LionessSw::CameraId::PAYLOAD) ? payloadSlot_
                                                          : starSlot_;
    }

    void workerLoop(Slot& slot);
    void runJob(const Job& job);
    void sendCompletion(const Job& job, LionessSw::CaptureStatus status,
                        std::uint32_t frameCounter, std::uint32_t frameBytes,
                        std::uint32_t previewBytes, std::uint32_t losslessBytes,
                        std::uint32_t pngBytes, std::uint64_t checksum,
                        const Fw::Buffer& preview, const Fw::Buffer& lossless,
                        const Fw::Buffer& png, const Fw::Buffer& source);
    void finishJob(LionessSw::CameraId cameraId,
                   LionessSw::CaptureStatus status);

    CameraHal::ICameraDevice* payloadDevice_;
    CameraHal::ICameraDevice* starDevice_;
    std::string payloadDevicePath_;
    std::string starDevicePath_;
    std::uint32_t payloadWidth_;
    std::uint32_t payloadHeight_;
    std::uint32_t starWidth_;
    std::uint32_t starHeight_;
    std::atomic<std::uint32_t> nextRequestId_;
    // Counters are touched from the worker thread and from the
    // component task thread (processDone), so they are atomic.
    std::atomic<std::uint32_t> capturesTaken_;
    std::atomic<std::uint32_t> capturesFailed_;
    std::atomic<std::uint32_t> capturesBusy_;

    // Per-device job slots. The pending process job lives in
    // the slot of the device it came from, so two processed
    // jobs, one per camera, never share a record. Written by
    // the worker threads, read by the processDone handler on
    // the component task thread. Guarded by mutex_.
    Slot payloadSlot_;
    Slot starSlot_;

    std::mutex mutex_;
    bool stop_;
};

}  // namespace Components

#endif
