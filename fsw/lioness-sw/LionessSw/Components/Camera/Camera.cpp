// Camera.cpp
// See Camera.hpp. The worker thread polls the device with a
// timeout; the component task thread only handles commands,
// submits and completions. Nothing on a manager thread ever
// blocks on capture or encoding.
// Style rule for this project: plain punctuation only, no em dashes.

#include "LionessSw/Components/Camera/Camera.hpp"

#include "LionessSw/V4L2/videodev2_compat.h"

#include "LionessSw/V4L2/SimCameraDevice.hpp"

namespace Components {

Camera::Camera(const char* const compName)
    : CameraComponentBase(compName), payloadDevice_(nullptr),
      starDevice_(nullptr), payloadWidth_(0), payloadHeight_(0),
      starWidth_(0), starHeight_(0), nextRequestId_(1), capturesTaken_(0),
      capturesFailed_(0), capturesBusy_(0), stop_(false) {
    // One worker thread per camera, so a payload capture and a
    // star capture can run at the same time.
    payloadSlot_.worker = std::thread(&Camera::workerLoop, this,
                                      std::ref(payloadSlot_));
    starSlot_.worker = std::thread(&Camera::workerLoop, this,
                                   std::ref(starSlot_));
}

Camera::~Camera() {
    shutdownWorker();
}

void Camera::shutdownWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    payloadSlot_.hasJob.notify_one();
    starSlot_.hasJob.notify_one();
    if (payloadSlot_.worker.joinable()) payloadSlot_.worker.join();
    if (starSlot_.worker.joinable()) starSlot_.worker.join();
}

// ----------------------------------------------------------------------
// Shared submit path
// ----------------------------------------------------------------------

LionessSw::CaptureStatus Camera::submit(const Job& job) {
    CameraHal::ICameraDevice* device =
        (job.cameraId == LionessSw::CameraId::PAYLOAD) ? payloadDevice_
                                                       : starDevice_;
    if (device == nullptr) {
        this->log_WARNING_LO_CaptureInvalid(job.requestId);
        return LionessSw::CaptureStatus::INVALID;
    }
    if (!CameraHal::isSupportedMode(job.width, job.height)) {
        this->log_WARNING_LO_CaptureInvalid(job.requestId);
        return LionessSw::CaptureStatus::INVALID;
    }
    if (job.timeoutMs == 0 || job.timeoutMs > 60000) {
        this->log_WARNING_LO_CaptureInvalid(job.requestId);
        return LionessSw::CaptureStatus::INVALID;
    }

    // The busy flag belongs to the device this job is for. A
    // payload capture does not block a star capture, and a star
    // capture does not block a payload capture.
    Slot& slot = slotFor(job.cameraId);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (slot.busy.load()) {
            std::uint32_t busy = ++capturesBusy_;
            this->tlmWrite_CapturesBusy(busy);
            this->log_WARNING_LO_CaptureBusy(job.requestId);
            return LionessSw::CaptureStatus::BUSY;
        }
        slot.busy.store(true);
        slot.pendingJob = job;
        slot.hasPendingJob = true;
    }
    this->log_ACTIVITY_HI_CaptureStarted(job.requestId, job.cameraId);
    slot.hasJob.notify_one();
    return LionessSw::CaptureStatus::OK;
}

void Camera::finishJob(LionessSw::CameraId cameraId,
                       LionessSw::CaptureStatus status) {
    // Clear the busy flag for this device before any completion
    // goes out, so a caller that submits again from inside the
    // completion never sees a stale Busy.
    slotFor(cameraId).busy.store(false);
    if (status == LionessSw::CaptureStatus::OK) {
        std::uint32_t taken = ++capturesTaken_;
        this->tlmWrite_CapturesTaken(taken);
    } else {
        std::uint32_t failed = ++capturesFailed_;
        this->tlmWrite_CapturesFailed(failed);
    }
}

void Camera::sendCompletion(const Job& job, LionessSw::CaptureStatus status,
                            std::uint32_t frameCounter, std::uint32_t frameBytes,
                            std::uint32_t previewBytes,
                            std::uint32_t losslessBytes, std::uint32_t pngBytes,
                            std::uint64_t checksum, const Fw::Buffer& preview,
                            const Fw::Buffer& lossless, const Fw::Buffer& png,
                            const Fw::Buffer& source) {
    if (job.cameraId == LionessSw::CameraId::PAYLOAD) {
        if (this->isConnected_captureDonePayload_OutputPort(0)) {
            this->captureDonePayload_out(0, job.requestId, status, job.cameraId,
                                         job.width, job.height, frameBytes,
                                         frameCounter, previewBytes, losslessBytes,
                                         pngBytes, checksum, preview, lossless,
                                         png, source);
        }
    } else {
        if (this->isConnected_captureDoneStar_OutputPort(0)) {
            this->captureDoneStar_out(0, job.requestId, status, job.cameraId,
                                      job.width, job.height, frameBytes,
                                      frameCounter, previewBytes, losslessBytes,
                                      pngBytes, checksum, preview, lossless,
                                      png, source);
        }
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

LionessSw::CaptureStatus Camera::captureSubmitPayload_handler(
    FwIndexType portNum, std::uint32_t requestId,
    const LionessSw::CameraId& cameraId, std::uint32_t width,
    std::uint32_t height, const LionessSw::CaptureOperation& operation,
    std::uint32_t timeoutMs) {
    (void)portNum;
    (void)cameraId;
    Job job;
    job.requestId = requestId;
    job.cameraId = LionessSw::CameraId::PAYLOAD;
    job.width = width;
    job.height = height;
    job.operation = operation;
    job.timeoutMs = timeoutMs;
    return submit(job);
}

LionessSw::CaptureStatus Camera::captureSubmitStar_handler(
    FwIndexType portNum, std::uint32_t requestId,
    const LionessSw::CameraId& cameraId, std::uint32_t width,
    std::uint32_t height, const LionessSw::CaptureOperation& operation,
    std::uint32_t timeoutMs) {
    (void)portNum;
    (void)cameraId;
    Job job;
    job.requestId = requestId;
    job.cameraId = LionessSw::CameraId::STAR_TRACKER;
    job.width = width;
    job.height = height;
    job.operation = operation;
    job.timeoutMs = timeoutMs;
    return submit(job);
}

void Camera::processDone_handler(FwIndexType portNum, std::uint32_t requestId,
                                 const LionessSw::CameraId& cameraId,
                                 const LionessSw::CaptureStatus& status,
                                 std::uint32_t width, std::uint32_t height,
                                 std::uint32_t previewBytes,
                                 std::uint32_t losslessBytes,
                                 std::uint32_t pngBytes,
                                 std::uint64_t sourceChecksum,
                                 const Fw::Buffer& preview,
                                 const Fw::Buffer& lossless,
                                 const Fw::Buffer& png) {
    (void)portNum;
    (void)width;
    (void)height;
    Job job;
    CameraHal::ICameraDevice* device = nullptr;
    std::uint32_t bufIndex = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // The pending process job lives in the slot of the
        // device it came from. Match on camera and request:
        // the two managers number requests independently, so
        // a requestId alone is not unique across cameras.
        Slot& slot = slotFor(cameraId);
        if (!slot.hasPendingProcessJob ||
            slot.pendingProcessJob.requestId != requestId) {
            // No matching job. This should not happen while the
            // per-device one-job rule holds, but never crash
            // on it.
            this->log_WARNING_LO_UnknownBufferReturned();
            return;
        }
        job = slot.pendingProcessJob;
        device = slot.pendingProcessDevice;
        bufIndex = slot.pendingProcessBufIndex;
        slot.hasPendingProcessJob = false;
    }
    // The source frame has been read by ImageProcessing. Hand the
    // device buffer back before the completion goes out.
    if (device != nullptr) {
        (void)device->requeueBuffer(bufIndex);
    }
    finishJob(job.cameraId, status);
    if (status == LionessSw::CaptureStatus::OK) {
        this->log_ACTIVITY_HI_CaptureComplete(requestId, job.frameCounter);
    } else {
        this->log_WARNING_HI_CaptureFailed(requestId, status);
    }
    Fw::Buffer empty;
    sendCompletion(job, status, job.frameCounter, job.frameBytes,
                   previewBytes, losslessBytes, pngBytes, sourceChecksum,
                   preview, lossless, png, empty);
}

void Camera::returnFrame_handler(FwIndexType portNum,
                                 const Fw::Buffer& buffer) {
    (void)portNum;
    std::uint32_t context = buffer.getContext();
    std::uint32_t bufIndex = context & 0xFFFF;
    CameraHal::ICameraDevice* device =
        ((context & 0x10000) != 0) ? starDevice_ : payloadDevice_;
    if (device == nullptr || bufIndex >= 8) {
        this->log_WARNING_LO_UnknownBufferReturned();
        return;
    }
    (void)device->requeueBuffer(bufIndex);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Camera::TakePicture_cmdHandler(FwOpcodeType opCode, U32 cmdSeq,
                                    LionessSw::CameraId cameraId, U32 width,
                                    U32 height,
                                    LionessSw::CaptureOperation operation,
                                    U32 timeoutMs) {
    Job job;
    job.requestId = nextRequestId_.fetch_add(1);
    job.cameraId = cameraId;
    job.width = width;
    job.height = height;
    job.operation = operation;
    job.timeoutMs = timeoutMs;
    LionessSw::CaptureStatus status = submit(job);
    if (status == LionessSw::CaptureStatus::OK) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } else {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

// ----------------------------------------------------------------------
// Worker thread
// ----------------------------------------------------------------------

bool Camera::configureDevice(CameraHal::ICameraDevice* device,
                             const std::string& devicePath,
                             std::uint32_t width, std::uint32_t height,
                             std::uint32_t& configuredWidth,
                             std::uint32_t& configuredHeight) {
    if (device->isStreaming() && configuredWidth == width &&
        configuredHeight == height) {
        return true;
    }
    if (!device->isOpen()) {
        if (device->open(devicePath) != 0) return false;
    }
    if (device->isStreaming()) {
        if (device->stopStreaming() != 0) return false;
    }
    if (device->setFormat(width, height, V4L2_PIX_FMT_SRGGB10) != 0) {
        return false;
    }
    if (device->allocateBuffers(kRingBuffers) != 0) {
        return false;
    }
    if (device->startStreaming() != 0) {
        return false;
    }
    configuredWidth = width;
    configuredHeight = height;
    return true;
}

void Camera::workerLoop(Slot& slot) {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            slot.hasJob.wait(lock,
                             [this, &slot] { return stop_ || slot.hasPendingJob; });
            if (stop_) return;
            job = slot.pendingJob;
            slot.hasPendingJob = false;
        }
        runJob(job);
    }
}

void Camera::runJob(const Job& job) {
    CameraHal::ICameraDevice* device =
        (job.cameraId == LionessSw::CameraId::PAYLOAD) ? payloadDevice_
                                                       : starDevice_;
    std::uint32_t& configuredWidth =
        (job.cameraId == LionessSw::CameraId::PAYLOAD) ? payloadWidth_ : starWidth_;
    std::uint32_t& configuredHeight =
        (job.cameraId == LionessSw::CameraId::PAYLOAD) ? payloadHeight_
                                                       : starHeight_;
    Fw::Buffer empty;

    const std::string& devicePath =
        (job.cameraId == LionessSw::CameraId::PAYLOAD) ? payloadDevicePath_
                                                       : starDevicePath_;
    if (device == nullptr ||
        !configureDevice(device, devicePath, job.width, job.height,
                         configuredWidth, configuredHeight)) {
        finishJob(job.cameraId, LionessSw::CaptureStatus::CAPTURE_ERROR);
        this->log_WARNING_HI_CaptureFailed(job.requestId,
                                           LionessSw::CaptureStatus::CAPTURE_ERROR);
        sendCompletion(job, LionessSw::CaptureStatus::CAPTURE_ERROR, 0, 0, 0, 0,
                       0, 0, empty, empty, empty, empty);
        return;
    }

    // Poll with a timeout. This is the fixed HAL call: it never
    // blocks longer than timeoutMs.
    CameraHal::DeviceFrame frame = device->dequeueFrame(static_cast<int>(job.timeoutMs));
    if (!frame.valid) {
        finishJob(job.cameraId, LionessSw::CaptureStatus::TIMEOUT);
        this->log_WARNING_HI_CaptureTimeout(job.requestId);
        sendCompletion(job, LionessSw::CaptureStatus::TIMEOUT, 0, 0, 0, 0, 0,
                       0, empty, empty, empty, empty);
        return;
    }

    std::uint32_t frameBytes = frame.size;
    std::uint64_t checksum = CameraHal::fnv1a(frame.data, frame.size);
    std::uint32_t context =
        ((job.cameraId == LionessSw::CameraId::STAR_TRACKER) ? 0x10000 : 0) |
        frame.bufIndex;
    Fw::Buffer source;
    source.setData(const_cast<std::uint8_t*>(frame.data));
    source.setSize(frame.size);
    source.setContext(context);

    if (job.operation == LionessSw::CaptureOperation::RAW) {
        // No processing. The raw frame goes straight to the
        // StarTrackerManager, which returns it through
        // returnFrame when done.
        finishJob(job.cameraId, LionessSw::CaptureStatus::OK);
        this->log_ACTIVITY_HI_CaptureComplete(job.requestId, frame.sequence);
        sendCompletion(job, LionessSw::CaptureStatus::OK, frame.sequence,
                       frameBytes, 0, 0, 0, checksum, empty, empty, empty,
                       source);
        return;
    }

    // Processed job: hand the frame to ImageProcessing and wait
    // for its completion on the component task thread.
    if (!this->isConnected_processImage_OutputPort(0)) {
        (void)device->requeueBuffer(frame.bufIndex);
        finishJob(job.cameraId, LionessSw::CaptureStatus::CAPTURE_ERROR);
        this->log_WARNING_HI_CaptureFailed(job.requestId,
                                           LionessSw::CaptureStatus::CAPTURE_ERROR);
        sendCompletion(job, LionessSw::CaptureStatus::CAPTURE_ERROR, 0, 0, 0, 0,
                       0, 0, empty, empty, empty, empty);
        return;
    }
    // Record the pending job in this device's slot BEFORE
    // calling processImage. The completion can arrive as soon
    // as the call returns, and a fast reply must find the job
    // already recorded, or the processDone handler would drop
    // it as unknown.
    Slot& slot = slotFor(job.cameraId);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Job pending = job;
        pending.frameCounter = frame.sequence;
        pending.frameBytes = frameBytes;
        slot.pendingProcessJob = pending;
        slot.hasPendingProcessJob = true;
        slot.pendingProcessDevice = device;
        slot.pendingProcessBufIndex = frame.bufIndex;
    }
    LionessSw::CaptureStatus status = this->processImage_out(
        0, job.requestId, job.cameraId, job.width, job.height, job.operation,
        source);
    if (status != LionessSw::CaptureStatus::OK) {
        // No completion is coming for a rejected job, so clear
        // the pending record again.
        {
            std::lock_guard<std::mutex> lock(mutex_);
            slot.hasPendingProcessJob = false;
        }
        (void)device->requeueBuffer(frame.bufIndex);
        finishJob(job.cameraId, LionessSw::CaptureStatus::CAPTURE_ERROR);
        this->log_WARNING_HI_CaptureFailed(job.requestId, status);
        sendCompletion(job, LionessSw::CaptureStatus::CAPTURE_ERROR, 0, 0, 0, 0,
                       0, 0, empty, empty, empty, empty);
        return;
    }
    // The worker returns here. The ImageProcessing completion
    // arrives on processDone and finishes the job.
}

}  // namespace Components
