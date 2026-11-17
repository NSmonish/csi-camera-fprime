// ImageProcessingTester.cpp
// Unit tests for the ImageProcessing component. A real RAW10
// frame is generated with the simulated camera helper, submitted
// through the sync port, and the completion is checked: preview
// bytes, lossless bytes that decompress byte for byte, and an
// unchanged source checksum.
// Style rule for this project: plain punctuation only, no em dashes.

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <thread>

#include <Fw/Test/UnitTest.hpp>

#include "LionessSw/Components/ImageProcessing/ImageCodec.hpp"
#include "LionessSw/Components/ImageProcessing/test/ut/ImageProcessingTester.hpp"
#include "LionessSw/V4L2/SimCameraDevice.hpp"

// stb_image decodes the JPEG and PNG previews in the tests,
// so the test checks real image content, not just magic bytes.
#define STB_IMAGE_IMPLEMENTATION
#include "LionessSw/Components/ImageProcessing/stb_image.h"

namespace Components {

ImageProcessingTester::ImageProcessingTester(ImageProcessing& inst)
    : ImageProcessingGTestBase("testerbase", 100), m_impl(inst) {}

ImageProcessingTester::~ImageProcessingTester() {
    // Stop the worker first: no callback may run after this
    // tester is gone. Then free the message queue.
    this->m_impl.shutdownWorker();
    this->m_impl.deinit();
}

void ImageProcessingTester::from_processDone_handler(
    FwIndexType portNum, std::uint32_t requestId,
    const LionessSw::CameraId& cameraId,
    const LionessSw::CaptureStatus& status, std::uint32_t width,
    std::uint32_t height, std::uint32_t previewBytes,
    std::uint32_t losslessBytes, std::uint32_t pngBytes,
    std::uint64_t sourceChecksum, const Fw::Buffer& preview,
    const Fw::Buffer& lossless, const Fw::Buffer& png) {
    (void)portNum;
    (void)cameraId;
    this->pushFromPortEntry_processDone(requestId, cameraId, status, width,
                                       height, previewBytes, losslessBytes,
                                       pngBytes, sourceChecksum, preview,
                                       lossless, png);
    Completion c;
    c.requestId = requestId;
    c.status = status;
    c.width = width;
    c.height = height;
    c.previewBytes = previewBytes;
    c.losslessBytes = losslessBytes;
    c.pngBytes = pngBytes;
    c.sourceChecksum = sourceChecksum;
    if (preview.getData() != nullptr && preview.getSize() > 0) {
        c.preview.assign(preview.getData(),
                         preview.getData() + preview.getSize());
    }
    if (lossless.getData() != nullptr && lossless.getSize() > 0) {
        c.lossless.assign(lossless.getData(),
                          lossless.getData() + lossless.getSize());
    }
    if (png.getData() != nullptr && png.getSize() > 0) {
        c.png.assign(png.getData(), png.getData() + png.getSize());
    }
    c.previewBuffer = preview;
    c.losslessBuffer = lossless;
    c.pngBuffer = png;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_completions.push_back(c);
    }
    m_cv.notify_all();
}

std::vector<ImageProcessingTester::Completion>
ImageProcessingTester::getCompletions() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_completions;
}

bool ImageProcessingTester::waitForCompletions(std::size_t count,
                                               std::uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                         [this, count] { return m_completions.size() >= count; });
}

namespace {

// Build a source frame buffer holding a generated RAW10 frame.
struct SourceFrame {
    std::vector<std::uint8_t> bytes;
    Fw::Buffer buffer;

    SourceFrame(std::uint32_t width, std::uint32_t height,
                std::uint32_t counter) {
        bytes.resize(CameraHal::raw10FrameBytes(width, height));
        CameraHal::generateRaw10Frame(bytes.data(), width, height, counter);
        buffer.setData(bytes.data());
        buffer.setSize(static_cast<U32>(bytes.size()));
    }
};

}  // namespace

void ImageProcessingTester::testPreviewAndLossless() {
    TEST_CASE(201.1.1, "Preview plus lossless from one frame");
    this->clearEvents();
    this->clearTlm();
    SourceFrame frame(1280, 720, 5);
    std::uint64_t expectedChecksum =
        CameraHal::fnv1a(frame.bytes.data(), frame.bytes.size());

    LionessSw::CaptureStatus status = this->invoke_to_processImage(
        0, 1, LionessSw::CameraId::PAYLOAD, 1280, 720, LionessSw::CaptureOperation::BOTH, frame.buffer);
    ASSERT_EQ(status, LionessSw::CaptureStatus::OK);
    ASSERT_TRUE(this->waitForCompletions(1, 5000));

    std::vector<Completion> done = this->getCompletions();
    ASSERT_EQ(done.size(), 1u);
    ASSERT_EQ(done[0].status, LionessSw::CaptureStatus::OK);
    ASSERT_EQ(done[0].requestId, 1u);
    ASSERT_GT(done[0].previewBytes, 0u);
    ASSERT_GT(done[0].losslessBytes, 0u);
    ASSERT_EQ(done[0].sourceChecksum, expectedChecksum);

    // The lossless product must decompress byte for byte.
    std::vector<std::uint8_t> decoded(frame.bytes.size());
    std::size_t decodedBytes = 0;
    ASSERT_TRUE(ImageCodec::decompressLossless(
        done[0].lossless.data(), done[0].lossless.size(), decoded.data(),
        decoded.size(), decodedBytes));
    ASSERT_EQ(decodedBytes, frame.bytes.size());
    ASSERT_EQ(std::memcmp(decoded.data(), frame.bytes.data(),
                          frame.bytes.size()),
              0);

    // The JPEG preview starts with FF D8, and the PNG preview
    // starts with the PNG signature 89 50 4E 47. Both previews
    // are encoded by stb_image_write from the same downsampled
    // frame, so decoding both must give the same pixels.
    ASSERT_TRUE(ImageCodec::previewIsJpeg());
    ASSERT_EQ(done[0].preview.size(), done[0].previewBytes);
    ASSERT_EQ(done[0].preview[0], 0xFF);
    ASSERT_EQ(done[0].preview[1], 0xD8);
    ASSERT_GT(done[0].pngBytes, 0u);
    ASSERT_EQ(done[0].png.size(), done[0].pngBytes);
    ASSERT_EQ(done[0].png[0], 0x89);
    ASSERT_EQ(done[0].png[1], 0x50);
    ASSERT_EQ(done[0].png[2], 0x4E);
    ASSERT_EQ(done[0].png[3], 0x47);
    {
        int jpgW = 0, jpgH = 0, jpgC = 0;
        unsigned char* jpgPixels = stbi_load_from_memory(
            done[0].preview.data(),
            static_cast<int>(done[0].preview.size()), &jpgW, &jpgH, &jpgC, 1);
        ASSERT_TRUE(jpgPixels != nullptr);
        ASSERT_EQ(jpgW, 320);
        ASSERT_EQ(jpgH, 240);
        int pngW = 0, pngH = 0, pngC = 0;
        unsigned char* pngPixels = stbi_load_from_memory(
            done[0].png.data(), static_cast<int>(done[0].png.size()),
            &pngW, &pngH, &pngC, 1);
        ASSERT_TRUE(pngPixels != nullptr);
        ASSERT_EQ(pngW, 320);
        ASSERT_EQ(pngH, 240);
        // The PNG holds the exact downsampled pixels. The JPEG
        // is lossy at quality 30, so its pixels must only be
        // close on average: the sampled mean difference must be
        // at most 8 grey levels, and at least 90 percent of
        // sampled pixels must be within 16 grey levels. Measured
        // on this gradient frame: mean 4.6, 94 percent within
        // 16, worst 98 at the sharp sawtooth edges, where JPEG
        // ringing is worst. A different frame would fail both
        // bounds by a wide margin.
        {
            long totalDiff = 0;
            long samples = 0;
            long close = 0;
            for (int y = 0; y < 240; y += 17) {
                for (int x = 0; x < 320; x += 19) {
                    int i = y * 320 + x;
                    int diff = static_cast<int>(jpgPixels[i]) -
                               static_cast<int>(pngPixels[i]);
                    if (diff < 0) diff = -diff;
                    totalDiff += diff;
                    ++samples;
                    if (diff <= 16) ++close;
                }
            }
            ASSERT_LE(totalDiff / samples, 8);
            ASSERT_GE(close * 100 / samples, 90);
        }
        stbi_image_free(jpgPixels);
        stbi_image_free(pngPixels);
    }

    // Events and telemetry for one good job.
    ASSERT_EVENTS_JobComplete_SIZE(1);
    ASSERT_TLM_JobsCompleted_SIZE(1);

    // The output buffers are in use until returned.
    ASSERT_TRUE(this->m_impl.isPreviewInUse());
    ASSERT_TRUE(this->m_impl.isPngInUse());
    ASSERT_TRUE(this->m_impl.isLosslessInUse());

    // Return the buffers through the port and check they free.
    this->invoke_to_returnBuffer(0, done[0].previewBuffer);
    this->invoke_to_returnBuffer(0, done[0].pngBuffer);
    this->invoke_to_returnBuffer(0, done[0].losslessBuffer);
    ASSERT_FALSE(this->m_impl.isPreviewInUse());
    ASSERT_FALSE(this->m_impl.isPngInUse());
    ASSERT_FALSE(this->m_impl.isLosslessInUse());

    // With the buffers back, a new job is accepted again.
    SourceFrame frame2(640, 480, 6);
    LionessSw::CaptureStatus status2 = this->invoke_to_processImage(
        0, 2, LionessSw::CameraId::PAYLOAD, 640, 480, LionessSw::CaptureOperation::BOTH, frame2.buffer);
    ASSERT_EQ(status2, LionessSw::CaptureStatus::OK);
    ASSERT_TRUE(this->waitForCompletions(2, 5000));
}

void ImageProcessingTester::testBusy() {
    TEST_CASE(201.2.1, "Second submit while busy returns Busy");
    this->clearEvents();
    this->clearTlm();
    SourceFrame frame(4608, 2592, 1);
    LionessSw::CaptureStatus first = this->invoke_to_processImage(
        0, 2, LionessSw::CameraId::PAYLOAD, 4608, 2592, LionessSw::CaptureOperation::BOTH, frame.buffer);
    ASSERT_EQ(first, LionessSw::CaptureStatus::OK);
    SourceFrame frame2(1280, 720, 2);
    LionessSw::CaptureStatus second = this->invoke_to_processImage(
        0, 3, LionessSw::CameraId::PAYLOAD, 1280, 720, LionessSw::CaptureOperation::BOTH, frame2.buffer);
    ASSERT_EQ(second, LionessSw::CaptureStatus::BUSY);
    ASSERT_TRUE(this->waitForCompletions(1, 10000));
    ASSERT_EVENTS_JobBusy_SIZE(1);
    ASSERT_TLM_JobsBusy_SIZE(1);
}

void ImageProcessingTester::testInvalid() {
    TEST_CASE(201.3.1, "Invalid mode rejected immediately");
    this->clearEvents();
    SourceFrame frame(1280, 720, 1);
    LionessSw::CaptureStatus status = this->invoke_to_processImage(
        0, 4, LionessSw::CameraId::PAYLOAD, 999, 999, LionessSw::CaptureOperation::BOTH, frame.buffer);
    ASSERT_EQ(status, LionessSw::CaptureStatus::INVALID);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(this->getCompletions().size(), 0u);
    ASSERT_EVENTS_JobInvalid_SIZE(1);
}

void ImageProcessingTester::testPreviewOnly() {
    TEST_CASE(201.4.1, "Preview only produces no lossless output");
    this->clearEvents();
    SourceFrame frame(1280, 720, 7);
    LionessSw::CaptureStatus status = this->invoke_to_processImage(
        0, 5, LionessSw::CameraId::PAYLOAD, 1280, 720, LionessSw::CaptureOperation::PREVIEW, frame.buffer);
    ASSERT_EQ(status, LionessSw::CaptureStatus::OK);
    ASSERT_TRUE(this->waitForCompletions(1, 5000));
    std::vector<Completion> done = this->getCompletions();
    ASSERT_EQ(done.size(), 1u);
    ASSERT_EQ(done[0].status, LionessSw::CaptureStatus::OK);
    ASSERT_GT(done[0].previewBytes, 0u);
    ASSERT_GT(done[0].pngBytes, 0u);
    ASSERT_EQ(done[0].losslessBytes, 0u);
}

void ImageProcessingTester::testLosslessOnly() {
    TEST_CASE(201.5.1, "Lossless only produces no preview output");
    this->clearEvents();
    SourceFrame frame(1280, 720, 8);
    LionessSw::CaptureStatus status = this->invoke_to_processImage(
        0, 6, LionessSw::CameraId::PAYLOAD, 1280, 720, LionessSw::CaptureOperation::LOSSLESS, frame.buffer);
    ASSERT_EQ(status, LionessSw::CaptureStatus::OK);
    ASSERT_TRUE(this->waitForCompletions(1, 5000));
    std::vector<Completion> done = this->getCompletions();
    ASSERT_EQ(done.size(), 1u);
    ASSERT_EQ(done[0].status, LionessSw::CaptureStatus::OK);
    ASSERT_EQ(done[0].previewBytes, 0u);
    ASSERT_EQ(done[0].pngBytes, 0u);
    ASSERT_GT(done[0].losslessBytes, 0u);
    std::vector<std::uint8_t> decoded(frame.bytes.size());
    std::size_t decodedBytes = 0;
    ASSERT_TRUE(ImageCodec::decompressLossless(
        done[0].lossless.data(), done[0].lossless.size(), decoded.data(),
        decoded.size(), decodedBytes));
    ASSERT_EQ(std::memcmp(decoded.data(), frame.bytes.data(),
                          frame.bytes.size()),
              0);
}

}  // namespace Components
