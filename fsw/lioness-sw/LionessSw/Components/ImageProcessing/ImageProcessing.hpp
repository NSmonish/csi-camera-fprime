// ImageProcessing.hpp
// ImageProcessing component implementation class (Issue 44).
// One active component. A short synchronous submit validates
// the request and reserves one job. A worker thread performs
// the encoding and sends one completion per accepted request.
// Output buffers are preallocated once, at construction, and
// are handed back by the receiver through returnBuffer.
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef Components_ImageProcessing_HPP
#define Components_ImageProcessing_HPP

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "LionessSw/Components/ImageProcessing/ImageProcessingComponentAc.hpp"

namespace Components {

class ImageProcessing final : public ImageProcessingComponentBase {
public:
    // Largest frame this component accepts: 4608x2592 RAW10.
    static const std::size_t kMaxFrameBytes = 14929920;
    // Preview buffer holds a JPEG, which can be larger than the
    // raw 320x240 grayscale image for noisy input at high quality.
    static const std::size_t kPreviewCapacity = 2 * 320 * 240;
    // PNG buffer holds the PNG preview of the same 320x240
    // grayscale frame. PNG is 8-bit, so it is a viewable
    // preview, not a lossless product.
    static const std::size_t kPngCapacity = 2 * 320 * 240;

    ImageProcessing(const char* const compName);
    ~ImageProcessing();

    // Stop the worker thread and wait for it to finish. Used by
    // tests so no worker callback can run after the tester that
    // receives it is destroyed. Safe to call more than once.
    void shutdownWorker();

    // For tests and for the topology: whether the preview, PNG
    // and lossless output buffers are currently out with a
    // receiver.
    bool isPreviewInUse() const { return previewInUse_.load(); }
    bool isPngInUse() const { return pngInUse_.load(); }
    bool isLosslessInUse() const { return losslessInUse_.load(); }
    std::uint32_t getQuality() const { return quality_.load(); }

private:
    struct Job {
        std::uint32_t requestId;
        LionessSw::CameraId cameraId;
        std::uint32_t width;
        std::uint32_t height;
        LionessSw::CaptureOperation operation;
        Fw::Buffer source;

        Job()
            : requestId(0), cameraId(LionessSw::CameraId::PAYLOAD), width(0),
              height(0), operation(LionessSw::CaptureOperation::BOTH) {}
    };

    // Handlers for the typed input ports and the command. The
    // signatures must match the generated base class exactly.
    LionessSw::CaptureStatus processImage_handler(
        FwIndexType portNum, std::uint32_t requestId,
        const LionessSw::CameraId& cameraId, std::uint32_t width,
        std::uint32_t height, const LionessSw::CaptureOperation& operation,
        const Fw::Buffer& source) override;
    void returnBuffer_handler(FwIndexType portNum,
                              const Fw::Buffer& buffer) override;
    void SetPreviewQuality_cmdHandler(FwOpcodeType opCode, U32 cmdSeq,
                                      U32 quality) override;

    void workerLoop();
    void runJob(const Job& job);

    // quality_ and the in-use flags are touched from both the
    // component task thread and the worker thread, so they are
    // atomic. The job counters are only touched by the worker.
    std::atomic<std::uint32_t> quality_;
    std::uint32_t jobsCompleted_;
    std::uint32_t jobsFailed_;
    std::uint32_t jobsBusy_;

    // Preallocated output buffers. Nothing allocates during a job.
    std::vector<std::uint8_t> previewBuffer_;
    std::vector<std::uint8_t> pngBuffer_;
    std::vector<std::uint8_t> losslessBuffer_;
    std::vector<std::uint8_t> scratch_;
    std::atomic<bool> previewInUse_;
    std::atomic<bool> pngInUse_;
    std::atomic<bool> losslessInUse_;

    std::mutex mutex_;
    std::condition_variable hasJob_;
    bool stop_;
    bool busy_;
    bool hasPendingJob_;
    Job pendingJob_;
    std::thread worker_;
};

}  // namespace Components

#endif
