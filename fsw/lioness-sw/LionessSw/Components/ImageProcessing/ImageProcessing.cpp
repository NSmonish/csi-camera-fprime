// ImageProcessing.cpp
// See ImageProcessing.hpp and sdd.md (Issue 44).
// Style rule for this project: plain punctuation only, no em dashes.

#include "LionessSw/Components/ImageProcessing/ImageProcessing.hpp"

#include "LionessSw/Components/ImageProcessing/ImageCodec.hpp"
#include "LionessSw/V4L2/SimCameraDevice.hpp"

namespace Components {

ImageProcessing::ImageProcessing(const char* const compName)
    : ImageProcessingComponentBase(compName), quality_(30), jobsCompleted_(0),
      jobsFailed_(0), jobsBusy_(0),
      previewBuffer_(kPreviewCapacity, 0), pngBuffer_(kPngCapacity, 0),
      losslessBuffer_(ImageCodec::zlibBound(kMaxFrameBytes), 0),
      scratch_(320 * 240, 0), previewInUse_(false), pngInUse_(false),
      losslessInUse_(false), stop_(false), busy_(false), hasPendingJob_(false),
      worker_(&ImageProcessing::workerLoop, this) {}

ImageProcessing::~ImageProcessing() {
    shutdownWorker();
}

void ImageProcessing::shutdownWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    hasJob_.notify_one();
    if (worker_.joinable()) worker_.join();
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

LionessSw::CaptureStatus ImageProcessing::processImage_handler(
    FwIndexType portNum, std::uint32_t requestId,
    const LionessSw::CameraId& cameraId, std::uint32_t width,
    std::uint32_t height, const LionessSw::CaptureOperation& operation,
    const Fw::Buffer& source) {
    (void)portNum;
    // Validate before reserving anything. Invalid requests start
    // no worker work and retain no buffer.
    if (operation == LionessSw::CaptureOperation::RAW) {
        this->log_WARNING_LO_JobInvalid(requestId);
        return LionessSw::CaptureStatus::INVALID;
    }
    if (!CameraHal::isSupportedMode(width, height)) {
        this->log_WARNING_LO_JobInvalid(requestId);
        return LionessSw::CaptureStatus::INVALID;
    }
    if (source.getData() == nullptr ||
        source.getSize() < CameraHal::raw10FrameBytes(width, height)) {
        this->log_WARNING_LO_JobInvalid(requestId);
        return LionessSw::CaptureStatus::INVALID;
    }
    if (previewInUse_.load() || pngInUse_.load() || losslessInUse_.load()) {
        // An earlier output was never returned. Refuse the new
        // job rather than overwriting a buffer still in use.
        this->log_WARNING_LO_JobBusy(requestId);
        return LionessSw::CaptureStatus::BUSY;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (busy_) {
        ++jobsBusy_;
        this->tlmWrite_JobsBusy(jobsBusy_);
        this->log_WARNING_LO_JobBusy(requestId);
        return LionessSw::CaptureStatus::BUSY;
    }
    busy_ = true;
    pendingJob_.requestId = requestId;
    pendingJob_.cameraId = cameraId;
    pendingJob_.width = width;
    pendingJob_.height = height;
    pendingJob_.operation = operation;
    pendingJob_.source = source;
    hasPendingJob_ = true;
    hasJob_.notify_one();
    return LionessSw::CaptureStatus::OK;
}

void ImageProcessing::returnBuffer_handler(FwIndexType portNum,
                                           const Fw::Buffer& buffer) {
    (void)portNum;
    const std::uint8_t* data = buffer.getData();
    if (data == previewBuffer_.data()) {
        previewInUse_.store(false);
        return;
    }
    if (data == pngBuffer_.data()) {
        pngInUse_.store(false);
        return;
    }
    if (data == losslessBuffer_.data()) {
        losslessInUse_.store(false);
        return;
    }
    this->log_WARNING_LO_UnknownBufferReturned();
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void ImageProcessing::SetPreviewQuality_cmdHandler(FwOpcodeType opCode,
                                                   U32 cmdSeq, U32 quality) {
    if (quality < 1 || quality > 100) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    quality_.store(quality);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Worker thread
// ----------------------------------------------------------------------

void ImageProcessing::workerLoop() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            hasJob_.wait(lock, [this] { return stop_ || hasPendingJob_; });
            if (stop_) return;
            job = pendingJob_;
            hasPendingJob_ = false;
        }
        runJob(job);
    }
}

void ImageProcessing::runJob(const Job& job) {
    const std::uint8_t* source = job.source.getData();
    std::size_t frameBytes =
        CameraHal::raw10FrameBytes(job.width, job.height);
    std::uint64_t checksum = CameraHal::fnv1a(source, frameBytes);

    std::size_t previewBytes = 0;
    std::size_t pngBytes = 0;
    std::size_t losslessBytes = 0;
    bool isJpeg = false;
    bool ok = true;

    if (job.operation == LionessSw::CaptureOperation::PREVIEW ||
        job.operation == LionessSw::CaptureOperation::BOTH) {
        if (!ImageCodec::makePreview(source, frameBytes, job.width, job.height,
                                     quality_.load(), previewBuffer_.data(),
                                     previewBuffer_.size(), previewBytes, isJpeg,
                                     scratch_.data())) {
            ok = false;
        } else {
            previewInUse_.store(true);
        }
        // The PNG preview shows the same downsampled frame as
        // the JPEG. It is 8-bit, so it is a viewable preview,
        // not a lossless product.
        if (ok && !ImageCodec::makePreviewPng(source, frameBytes, job.width,
                                              job.height, pngBuffer_.data(),
                                              pngBuffer_.size(), pngBytes,
                                              scratch_.data())) {
            ok = false;
        } else if (ok) {
            pngInUse_.store(true);
        }
    }
    if (ok && (job.operation == LionessSw::CaptureOperation::LOSSLESS ||
               job.operation == LionessSw::CaptureOperation::BOTH)) {
        if (!ImageCodec::compressLossless(source, frameBytes,
                                          losslessBuffer_.data(),
                                          losslessBuffer_.size(), losslessBytes)) {
            ok = false;
        } else {
            losslessInUse_.store(true);
        }
    }

    // The source must be unchanged after encoding.
    std::uint64_t checksumAfter = CameraHal::fnv1a(source, frameBytes);
    if (checksumAfter != checksum) {
        ok = false;
    }
    if (!ok) {
        previewBytes = 0;
        pngBytes = 0;
        losslessBytes = 0;
        previewInUse_.store(false);
        pngInUse_.store(false);
        losslessInUse_.store(false);
    }

    LionessSw::CaptureStatus status =
        ok ? LionessSw::CaptureStatus::OK
           : LionessSw::CaptureStatus::CAPTURE_ERROR;

    Fw::Buffer previewBuf;
    Fw::Buffer pngBuf;
    Fw::Buffer losslessBuf;
    if (previewBytes > 0) {
        previewBuf.setData(previewBuffer_.data());
        previewBuf.setSize(static_cast<U32>(previewBytes));
    }
    if (pngBytes > 0) {
        pngBuf.setData(pngBuffer_.data());
        pngBuf.setSize(static_cast<U32>(pngBytes));
    }
    if (losslessBytes > 0) {
        losslessBuf.setData(losslessBuffer_.data());
        losslessBuf.setSize(static_cast<U32>(losslessBytes));
    }

    // Clear the busy flag before the completion goes out, so a
    // caller that submits again from inside the completion never
    // sees a stale Busy.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        busy_ = false;
    }
    if (ok) {
        ++jobsCompleted_;
        this->tlmWrite_JobsCompleted(jobsCompleted_);
        this->tlmWrite_PreviewIsJpeg(ImageCodec::previewIsJpeg() ? 1 : 0);
        this->log_ACTIVITY_HI_JobComplete(job.requestId,
                                          static_cast<U32>(previewBytes),
                                          static_cast<U32>(losslessBytes));
    } else {
        ++jobsFailed_;
        this->tlmWrite_JobsFailed(jobsFailed_);
        this->log_WARNING_HI_JobFailed(job.requestId, status);
    }
    if (this->isConnected_processDone_OutputPort(0)) {
        this->processDone_out(0, job.requestId, job.cameraId, status,
                              job.width, job.height,
                              static_cast<U32>(previewBytes),
                              static_cast<U32>(losslessBytes),
                              static_cast<U32>(pngBytes), checksum,
                              previewBuf, losslessBuf, pngBuf);
    }
}

}  // namespace Components
