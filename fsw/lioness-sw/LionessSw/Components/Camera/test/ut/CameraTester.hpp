// CameraTester.hpp
// Unit test harness for the Camera component. The tester plays
// the ImageProcessing component (from_processImage) and the two
// managers (from_captureDonePayload, from_captureDoneStar), and
// the real simulated devices supply the frames.
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef LionessSw_Components_Camera_test_ut_CameraTester_HPP
#define LionessSw_Components_Camera_test_ut_CameraTester_HPP

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "CameraGTestBase.hpp"
#include "LionessSw/Components/Camera/Camera.hpp"

namespace Components {

class CameraTester : public CameraGTestBase {
public:
    explicit CameraTester(Camera& inst);
    ~CameraTester();

    void testPayloadBoth();
    void testStarRaw();
    void testTimeout();
    void testBusy();
    void testInvalid();
    void testTakePictureCommand();
    void testConcurrentBoth();

    struct ProcessCall {
        std::uint32_t requestId;
        std::uint32_t width;
        std::uint32_t height;
        LionessSw::CaptureOperation operation;
        std::vector<std::uint8_t> sourceBytes;
    };

    struct Completion {
        std::uint32_t requestId;
        LionessSw::CaptureStatus status;
        LionessSw::CameraId cameraId;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t frameBytes;
        std::uint32_t frameCounter;
        std::uint32_t previewBytes;
        std::uint32_t losslessBytes;
        std::uint32_t pngBytes;
        std::uint64_t sourceChecksum;
        Fw::Buffer preview;
        Fw::Buffer lossless;
        Fw::Buffer png;
        Fw::Buffer source;
    };

    std::vector<ProcessCall> getProcessCalls();
    std::vector<Completion> getPayloadCompletions();
    std::vector<Completion> getStarCompletions();
    bool waitForProcessCall(std::uint32_t timeoutMs);
    bool waitForPayloadCompletion(std::uint32_t timeoutMs);
    bool waitForStarCompletion(std::uint32_t timeoutMs);

    // What from_processImage returns. Tests set BUSY to check the
    // Camera error path.
    void setProcessImageReturn(LionessSw::CaptureStatus status) {
        m_processImageReturn = status;
    }

    // Run the component task dispatch until the predicate holds
    // or the timeout passes. Returns true when it holds.
    template <typename Pred>
    bool dispatchUntil(Pred pred, std::uint32_t timeoutMs) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - start)
                   .count() < timeoutMs) {
            this->m_impl.doDispatch();
            if (pred()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return pred();
    }

private:
    void from_captureDonePayload_handler(
        FwIndexType portNum, std::uint32_t requestId,
        const LionessSw::CaptureStatus& status,
        const LionessSw::CameraId& cameraId, std::uint32_t width,
        std::uint32_t height, std::uint32_t frameBytes,
        std::uint32_t frameCounter, std::uint32_t previewBytes,
        std::uint32_t losslessBytes, std::uint32_t pngBytes,
        std::uint64_t sourceChecksum, const Fw::Buffer& preview,
        const Fw::Buffer& lossless, const Fw::Buffer& png,
        const Fw::Buffer& source) override;

    void from_captureDoneStar_handler(
        FwIndexType portNum, std::uint32_t requestId,
        const LionessSw::CaptureStatus& status,
        const LionessSw::CameraId& cameraId, std::uint32_t width,
        std::uint32_t height, std::uint32_t frameBytes,
        std::uint32_t frameCounter, std::uint32_t previewBytes,
        std::uint32_t losslessBytes, std::uint32_t pngBytes,
        std::uint64_t sourceChecksum, const Fw::Buffer& preview,
        const Fw::Buffer& lossless, const Fw::Buffer& png,
        const Fw::Buffer& source) override;

    LionessSw::CaptureStatus from_processImage_handler(
        FwIndexType portNum, std::uint32_t requestId,
        const LionessSw::CameraId& cameraId, std::uint32_t width,
        std::uint32_t height, const LionessSw::CaptureOperation& operation,
        const Fw::Buffer& source) override;

    // Answer a pending ImageProcessing job with fabricated
    // outputs, then dispatch the Camera task so its processDone
    // handler runs. A member function, so invoke_to_processDone
    // and doDispatch are accessible here.
    void answerProcessDone(std::uint32_t requestId, std::uint32_t width,
                           std::uint32_t height);

    Camera& m_impl;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<ProcessCall> m_processCalls;
    std::vector<Completion> m_payloadCompletions;
    std::vector<Completion> m_starCompletions;
    LionessSw::CaptureStatus m_processImageReturn;
};

}  // namespace Components

#endif
