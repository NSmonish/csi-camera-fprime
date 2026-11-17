// CameraTester.cpp
// Unit tests for the Camera component. The simulated devices
// supply real synthetic frames; the tester stands in for
// ImageProcessing and for the two managers.
// Style rule for this project: plain punctuation only, no em dashes.

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <thread>

#include <Fw/Test/UnitTest.hpp>

#include "LionessSw/Components/Camera/test/ut/CameraTester.hpp"
#include "LionessSw/V4L2/SimCameraDevice.hpp"

namespace Components {

CameraTester::CameraTester(Camera& inst)
    : CameraGTestBase("testerbase", 100), m_impl(inst),
      m_processImageReturn(LionessSw::CaptureStatus::OK) {}

CameraTester::~CameraTester() {
    // Stop the worker first: no callback may run after this
    // tester is gone. Then free the message queue.
    this->m_impl.shutdownWorker();
    this->m_impl.deinit();
}

void CameraTester::from_captureDonePayload_handler(
    FwIndexType portNum, std::uint32_t requestId,
    const LionessSw::CaptureStatus& status,
    const LionessSw::CameraId& cameraId, std::uint32_t width,
    std::uint32_t height, std::uint32_t frameBytes, std::uint32_t frameCounter,
    std::uint32_t previewBytes, std::uint32_t losslessBytes,
    std::uint32_t pngBytes, std::uint64_t sourceChecksum,
    const Fw::Buffer& preview, const Fw::Buffer& lossless,
    const Fw::Buffer& png, const Fw::Buffer& source) {
    (void)portNum;
    this->pushFromPortEntry_captureDonePayload(
        requestId, status, cameraId, width, height, frameBytes, frameCounter,
        previewBytes, losslessBytes, pngBytes, sourceChecksum, preview,
        lossless, png, source);
    Completion c;
    c.requestId = requestId;
    c.status = status;
    c.cameraId = cameraId;
    c.width = width;
    c.height = height;
    c.frameBytes = frameBytes;
    c.frameCounter = frameCounter;
    c.previewBytes = previewBytes;
    c.losslessBytes = losslessBytes;
    c.pngBytes = pngBytes;
    c.sourceChecksum = sourceChecksum;
    c.preview = preview;
    c.lossless = lossless;
    c.png = png;
    c.source = source;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_payloadCompletions.push_back(c);
    }
    m_cv.notify_all();
}

void CameraTester::from_captureDoneStar_handler(
    FwIndexType portNum, std::uint32_t requestId,
    const LionessSw::CaptureStatus& status,
    const LionessSw::CameraId& cameraId, std::uint32_t width,
    std::uint32_t height, std::uint32_t frameBytes, std::uint32_t frameCounter,
    std::uint32_t previewBytes, std::uint32_t losslessBytes,
    std::uint32_t pngBytes, std::uint64_t sourceChecksum,
    const Fw::Buffer& preview, const Fw::Buffer& lossless,
    const Fw::Buffer& png, const Fw::Buffer& source) {
    (void)portNum;
    this->pushFromPortEntry_captureDoneStar(
        requestId, status, cameraId, width, height, frameBytes, frameCounter,
        previewBytes, losslessBytes, pngBytes, sourceChecksum, preview,
        lossless, png, source);
    Completion c;
    c.requestId = requestId;
    c.status = status;
    c.cameraId = cameraId;
    c.width = width;
    c.height = height;
    c.frameBytes = frameBytes;
    c.frameCounter = frameCounter;
    c.previewBytes = previewBytes;
    c.losslessBytes = losslessBytes;
    c.pngBytes = pngBytes;
    c.sourceChecksum = sourceChecksum;
    c.preview = preview;
    c.lossless = lossless;
    c.png = png;
    c.source = source;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_starCompletions.push_back(c);
    }
    m_cv.notify_all();
}

LionessSw::CaptureStatus CameraTester::from_processImage_handler(
    FwIndexType portNum, std::uint32_t requestId,
    const LionessSw::CameraId& cameraId, std::uint32_t width,
    std::uint32_t height, const LionessSw::CaptureOperation& operation,
    const Fw::Buffer& source) {
    (void)portNum;
    (void)cameraId;
    this->pushFromPortEntry_processImage(requestId, cameraId, width, height,
                                        operation, source);
    ProcessCall call;
    call.requestId = requestId;
    call.width = width;
    call.height = height;
    call.operation = operation;
    if (source.getData() != nullptr && source.getSize() > 0) {
        call.sourceBytes.assign(source.getData(),
                                source.getData() + source.getSize());
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_processCalls.push_back(call);
    }
    m_cv.notify_all();
    return m_processImageReturn;
}

std::vector<CameraTester::ProcessCall> CameraTester::getProcessCalls() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_processCalls;
}

std::vector<CameraTester::Completion> CameraTester::getPayloadCompletions() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_payloadCompletions;
}

std::vector<CameraTester::Completion> CameraTester::getStarCompletions() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_starCompletions;
}

bool CameraTester::waitForProcessCall(std::uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                         [this] { return !m_processCalls.empty(); });
}

bool CameraTester::waitForPayloadCompletion(std::uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                         [this] { return !m_payloadCompletions.empty(); });
}

bool CameraTester::waitForStarCompletion(std::uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                         [this] { return !m_starCompletions.empty(); });
}

void CameraTester::answerProcessDone(std::uint32_t requestId,
                                     std::uint32_t width,
                                     std::uint32_t height) {
    static std::vector<std::uint8_t> preview(100, 0xAB);
    static std::vector<std::uint8_t> lossless(200, 0xCD);
    static std::vector<std::uint8_t> png(150, 0xEF);
    Fw::Buffer previewBuf;
    previewBuf.setData(preview.data());
    previewBuf.setSize(static_cast<U32>(preview.size()));
    Fw::Buffer losslessBuf;
    losslessBuf.setData(lossless.data());
    losslessBuf.setSize(static_cast<U32>(lossless.size()));
    Fw::Buffer pngBuf;
    pngBuf.setData(png.data());
    pngBuf.setSize(static_cast<U32>(png.size()));
    this->invoke_to_processDone(0, requestId, LionessSw::CameraId::PAYLOAD,
                                LionessSw::CaptureStatus::OK, width, height,
                                100, 200, 150, 12345, previewBuf, losslessBuf,
                                pngBuf);
    this->m_impl.doDispatch();
}

void CameraTester::testPayloadBoth() {
    TEST_CASE(202.1.1, "Payload capture through ImageProcessing");
    this->clearEvents();
    this->clearTlm();
    LionessSw::CaptureStatus status = this->invoke_to_captureSubmitPayload(
        0, 1, LionessSw::CameraId::PAYLOAD, 1280, 720,
        LionessSw::CaptureOperation::BOTH, 500);
    ASSERT_EQ(status, LionessSw::CaptureStatus::OK);
    ASSERT_TRUE(this->waitForProcessCall(5000));
    std::vector<ProcessCall> calls = this->getProcessCalls();
    ASSERT_EQ(calls.size(), 1u);
    ASSERT_EQ(calls[0].requestId, 1u);
    ASSERT_EQ(calls[0].width, 1280u);
    ASSERT_EQ(calls[0].height, 720u);
    // The source frame must be a real generated frame: its first
    // pixels carry the frame counter.
    ASSERT_EQ(calls[0].sourceBytes.size(),
              CameraHal::raw10FrameBytes(1280, 720));
    ASSERT_EQ(CameraHal::readFrameCounter(calls[0].sourceBytes.data(), 1280,
                                          720),
              1u);
    this->answerProcessDone(1, 1280, 720);
    ASSERT_TRUE(this->waitForPayloadCompletion(5000));
    std::vector<Completion> done = this->getPayloadCompletions();
    ASSERT_EQ(done.size(), 1u);
    ASSERT_EQ(done[0].status, LionessSw::CaptureStatus::OK);
    ASSERT_EQ(done[0].frameCounter, 1u);
    ASSERT_EQ(done[0].previewBytes, 100u);
    ASSERT_EQ(done[0].losslessBytes, 200u);
    ASSERT_EVENTS_CaptureComplete_SIZE(1);
    ASSERT_TLM_CapturesTaken_SIZE(1);
}

void CameraTester::testStarRaw() {
    TEST_CASE(202.2.1, "Star tracker raw capture, no processing");
    this->clearEvents();
    LionessSw::CaptureStatus status = this->invoke_to_captureSubmitStar(
        0, 2, LionessSw::CameraId::STAR_TRACKER, 1280, 720,
        LionessSw::CaptureOperation::RAW, 500);
    ASSERT_EQ(status, LionessSw::CaptureStatus::OK);
    ASSERT_TRUE(this->waitForStarCompletion(5000));
    std::vector<Completion> done = this->getStarCompletions();
    ASSERT_EQ(done.size(), 1u);
    ASSERT_EQ(done[0].status, LionessSw::CaptureStatus::OK);
    ASSERT_EQ(done[0].frameBytes,
              CameraHal::raw10FrameBytes(1280, 720));
    ASSERT_EQ(done[0].frameCounter, 1u);
    ASSERT_EQ(done[0].previewBytes, 0u);
    ASSERT_EQ(done[0].losslessBytes, 0u);
    ASSERT_EQ(done[0].source.getSize(), done[0].frameBytes);
    // No processing was requested.
    ASSERT_EQ(this->getProcessCalls().size(), 0u);
    // Return the frame buffer to the Camera.
    this->invoke_to_returnFrame(0, done[0].source);
    ASSERT_EVENTS_CaptureComplete_SIZE(1);
}

void CameraTester::testTimeout() {
    TEST_CASE(202.3.1, "Capture timeout gives one completion, zero bytes");
    this->clearEvents();
    CameraHal::SimCameraDevice* star =
        static_cast<CameraHal::SimCameraDevice*>(this->m_impl.getStarDevice());
    ASSERT_TRUE(star != nullptr);
    star->setFailTimeout(true);
    LionessSw::CaptureStatus status = this->invoke_to_captureSubmitStar(
        0, 3, LionessSw::CameraId::STAR_TRACKER, 1280, 720,
        LionessSw::CaptureOperation::RAW, 50);
    ASSERT_EQ(status, LionessSw::CaptureStatus::OK);
    ASSERT_TRUE(this->waitForStarCompletion(5000));
    std::vector<Completion> done = this->getStarCompletions();
    ASSERT_EQ(done.size(), 1u);
    ASSERT_EQ(done[0].status, LionessSw::CaptureStatus::TIMEOUT);
    ASSERT_EQ(done[0].previewBytes, 0u);
    ASSERT_EQ(done[0].losslessBytes, 0u);
    ASSERT_EQ(done[0].source.getSize(), 0u);
    ASSERT_EVENTS_CaptureTimeout_SIZE(1);
    ASSERT_TLM_CapturesFailed_SIZE(1);
    star->setFailTimeout(false);
}

void CameraTester::testBusy() {
    TEST_CASE(202.4.1, "Second submit while busy returns Busy");
    this->clearEvents();
    LionessSw::CaptureStatus first = this->invoke_to_captureSubmitPayload(
        0, 4, LionessSw::CameraId::PAYLOAD, 1280, 720,
        LionessSw::CaptureOperation::BOTH, 500);
    ASSERT_EQ(first, LionessSw::CaptureStatus::OK);
    LionessSw::CaptureStatus second = this->invoke_to_captureSubmitPayload(
        0, 5, LionessSw::CameraId::PAYLOAD, 1280, 720,
        LionessSw::CaptureOperation::BOTH, 500);
    ASSERT_EQ(second, LionessSw::CaptureStatus::BUSY);
    ASSERT_TRUE(this->waitForProcessCall(5000));
    this->answerProcessDone(4, 1280, 720);
    ASSERT_TRUE(this->waitForPayloadCompletion(5000));
    ASSERT_EQ(this->getPayloadCompletions().size(), 1u);
    ASSERT_EVENTS_CaptureBusy_SIZE(1);
}

void CameraTester::testConcurrentBoth() {
    TEST_CASE(202.7.1, "Payload and star captures run at the same time");
    this->clearEvents();
    // Submit a payload capture and a star capture back to back.
    // Each device has its own job slot and worker, so both must
    // be accepted, even though the payload job is still in
    // flight when the star job is submitted.
    LionessSw::CaptureStatus payloadStatus =
        this->invoke_to_captureSubmitPayload(
            0, 7, LionessSw::CameraId::PAYLOAD, 1280, 720,
            LionessSw::CaptureOperation::BOTH, 500);
    ASSERT_EQ(payloadStatus, LionessSw::CaptureStatus::OK);
    LionessSw::CaptureStatus starStatus = this->invoke_to_captureSubmitStar(
        0, 8, LionessSw::CameraId::STAR_TRACKER, 1280, 720,
        LionessSw::CaptureOperation::RAW, 500);
    ASSERT_EQ(starStatus, LionessSw::CaptureStatus::OK);
    // The star capture is raw, so it completes on its own
    // worker without waiting for ImageProcessing.
    ASSERT_TRUE(this->waitForStarCompletion(5000));
    std::vector<Completion> starDone = this->getStarCompletions();
    ASSERT_EQ(starDone.size(), 1u);
    ASSERT_EQ(starDone[0].status, LionessSw::CaptureStatus::OK);
    ASSERT_EQ(starDone[0].frameCounter, 1u);
    ASSERT_EQ(starDone[0].source.getSize(), starDone[0].frameBytes);
    this->invoke_to_returnFrame(0, starDone[0].source);
    // The payload capture went to ImageProcessing. Answer it
    // and check its completion, including the PNG product.
    ASSERT_TRUE(this->waitForProcessCall(5000));
    this->answerProcessDone(7, 1280, 720);
    ASSERT_TRUE(this->waitForPayloadCompletion(5000));
    std::vector<Completion> payloadDone = this->getPayloadCompletions();
    ASSERT_EQ(payloadDone.size(), 1u);
    ASSERT_EQ(payloadDone[0].status, LionessSw::CaptureStatus::OK);
    ASSERT_EQ(payloadDone[0].frameCounter, 1u);
    ASSERT_EQ(payloadDone[0].previewBytes, 100u);
    ASSERT_EQ(payloadDone[0].losslessBytes, 200u);
    ASSERT_EQ(payloadDone[0].pngBytes, 150u);
    // No Busy event: neither submit was rejected.
    ASSERT_EVENTS_CaptureBusy_SIZE(0);
    ASSERT_EVENTS_CaptureComplete_SIZE(2);
}

void CameraTester::testInvalid() {
    TEST_CASE(202.5.1, "Invalid mode rejected immediately");
    this->clearEvents();
    LionessSw::CaptureStatus status = this->invoke_to_captureSubmitPayload(
        0, 6, LionessSw::CameraId::PAYLOAD, 999, 999,
        LionessSw::CaptureOperation::BOTH, 500);
    ASSERT_EQ(status, LionessSw::CaptureStatus::INVALID);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(this->getPayloadCompletions().size(), 0u);
    ASSERT_EQ(this->getProcessCalls().size(), 0u);
    ASSERT_EVENTS_CaptureInvalid_SIZE(1);
}

void CameraTester::testTakePictureCommand() {
    TEST_CASE(202.6.1, "TakePicture command submits a capture");
    this->clearEvents();
    this->sendCmd_TakePicture(0, 0, LionessSw::CameraId::PAYLOAD, 1280, 720,
                              LionessSw::CaptureOperation::BOTH, 500);
    this->m_impl.doDispatch();
    ASSERT_TRUE(this->waitForProcessCall(5000));
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Camera::OPCODE_TAKEPICTURE, 0,
                        Fw::CmdResponse::OK);
}

}  // namespace Components
