// ImageProcessingTester.hpp
// Unit test harness for the ImageProcessing component.
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef LionessSw_Components_ImageProcessing_test_ut_ImageProcessingTester_HPP
#define LionessSw_Components_ImageProcessing_test_ut_ImageProcessingTester_HPP

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

#include "ImageProcessingGTestBase.hpp"
#include "LionessSw/Components/ImageProcessing/ImageProcessing.hpp"

namespace Components {

class ImageProcessingTester : public ImageProcessingGTestBase {
public:
    explicit ImageProcessingTester(ImageProcessing& inst);
    ~ImageProcessingTester();

    void testPreviewAndLossless();
    void testBusy();
    void testInvalid();
    void testPreviewOnly();
    void testLosslessOnly();

    struct Completion {
        std::uint32_t requestId;
        LionessSw::CaptureStatus status;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t previewBytes;
        std::uint32_t losslessBytes;
        std::uint32_t pngBytes;
        std::uint64_t sourceChecksum;
        std::vector<std::uint8_t> preview;
        std::vector<std::uint8_t> lossless;
        std::vector<std::uint8_t> png;
        // Buffer descriptors as sent by the component, pointing
        // at the component-owned output buffers. Used to return
        // the buffers through returnBuffer.
        Fw::Buffer previewBuffer;
        Fw::Buffer losslessBuffer;
        Fw::Buffer pngBuffer;
    };

    // Completions received on processDone, in order.
    std::vector<Completion> getCompletions();

    // Wait for at least count completions, up to timeoutMs.
    bool waitForCompletions(std::size_t count, std::uint32_t timeoutMs);

private:
    void from_processDone_handler(
        FwIndexType portNum, std::uint32_t requestId,
        const LionessSw::CameraId& cameraId,
        const LionessSw::CaptureStatus& status, std::uint32_t width,
        std::uint32_t height, std::uint32_t previewBytes,
        std::uint32_t losslessBytes, std::uint32_t pngBytes,
        std::uint64_t sourceChecksum, const Fw::Buffer& preview,
        const Fw::Buffer& lossless, const Fw::Buffer& png) override;

    ImageProcessing& m_impl;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<Completion> m_completions;
};

}  // namespace Components

#endif
