// StarTrackerManagerTester.cpp
// Unit tests for the StarTrackerManager component.
// Style rule for this project: plain punctuation only, no em dashes.

#include <gtest/gtest.h>

#include <Fw/Test/UnitTest.hpp>

#include "LionessSw/Components/StarTrackerManager/test/ut/StarTrackerManagerTester.hpp"

namespace Components {

StarTrackerManagerTester::StarTrackerManagerTester(StarTrackerManager& inst)
    : StarTrackerManagerGTestBase("testerbase", 100), m_impl(inst),
      m_submitReturn(LionessSw::CaptureStatus::OK) {}

StarTrackerManagerTester::~StarTrackerManagerTester() {
    this->m_impl.deinit();
}

LionessSw::CaptureStatus StarTrackerManagerTester::from_captureSubmit_handler(
    FwIndexType portNum, std::uint32_t requestId,
    const LionessSw::CameraId& cameraId, std::uint32_t width,
    std::uint32_t height, const LionessSw::CaptureOperation& operation,
    std::uint32_t timeoutMs) {
    (void)portNum;
    this->pushFromPortEntry_captureSubmit(requestId, cameraId, width, height,
                                         operation, timeoutMs);
    SubmitCall call;
    call.requestId = requestId;
    call.cameraId = cameraId;
    call.width = width;
    call.height = height;
    call.operation = operation;
    call.timeoutMs = timeoutMs;
    m_submitCalls.push_back(call);
    return m_submitReturn;
}

void StarTrackerManagerTester::from_returnFrame_handler(
    FwIndexType portNum, const Fw::Buffer& buffer) {
    (void)portNum;
    this->pushFromPortEntry_returnFrame(buffer);
    m_returnedFrames.push_back(buffer);
}

void StarTrackerManagerTester::from_processFrame_handler(
    FwIndexType portNum, const Fw::Buffer& frame, std::uint32_t width,
    std::uint32_t height, std::uint32_t frameCounter) {
    (void)portNum;
    this->pushFromPortEntry_processFrame(frame, width, height, frameCounter);
    FrameCall call;
    call.frame = frame;
    call.width = width;
    call.height = height;
    call.frameCounter = frameCounter;
    m_frameCalls.push_back(call);
}

void StarTrackerManagerTester::testTakeStarPicture() {
    TEST_CASE(204.1.1, "Take star picture delivers raw frame");
    this->clearEvents();
    this->clearTlm();
    this->sendCmd_TakeStarPicture(0, 0, 1280, 720);
    this->m_impl.doDispatch();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_EQ(this->getSubmitCalls().size(), 1u);
    ASSERT_EQ(this->getSubmitCalls()[0].cameraId,
              LionessSw::CameraId::STAR_TRACKER);
    ASSERT_EQ(this->getSubmitCalls()[0].operation,
              LionessSw::CaptureOperation::RAW);
    ASSERT_EQ(this->getSubmitCalls()[0].width, 1280u);
    ASSERT_EQ(this->getSubmitCalls()[0].height, 720u);

    std::vector<std::uint8_t> frameBytes(1152000, 0x55);
    Fw::Buffer source;
    source.setData(frameBytes.data());
    source.setSize(static_cast<U32>(frameBytes.size()));
    source.setContext(0);
    Fw::Buffer empty;
    this->invoke_to_captureDone(0, 1, LionessSw::CaptureStatus::OK,
                                LionessSw::CameraId::STAR_TRACKER, 1280, 720,
                                1152000, 7, 0, 0, 0, 0, empty, empty, empty,
                                source);
    this->m_impl.doDispatch();
    ASSERT_EVENTS_StarFrameDelivered_SIZE(1);
    ASSERT_TLM_StarFramesTaken_SIZE(1);
    ASSERT_EQ(this->getFrameCalls().size(), 1u);
    ASSERT_EQ(this->getFrameCalls()[0].frameCounter, 7u);
    ASSERT_EQ(this->getFrameCalls()[0].width, 1280u);
    ASSERT_EQ(this->getFrameCalls()[0].height, 720u);
    ASSERT_EQ(this->getFrameCalls()[0].frame.getData(), frameBytes.data());
    // The frame buffer was returned to the Camera.
    ASSERT_EQ(this->getReturnedFrames().size(), 1u);
    ASSERT_EQ(this->getReturnedFrames()[0].getData(), frameBytes.data());
}

void StarTrackerManagerTester::testCaptureFailed() {
    TEST_CASE(204.2.1, "Failed star capture delivers nothing");
    this->clearEvents();
    this->clearTlm();
    this->sendCmd_TakeStarPicture(0, 0, 1280, 720);
    this->m_impl.doDispatch();
    Fw::Buffer empty;
    this->invoke_to_captureDone(0, 1, LionessSw::CaptureStatus::TIMEOUT,
                                LionessSw::CameraId::STAR_TRACKER, 1280, 720,
                                0, 0, 0, 0, 0, 0, empty, empty, empty, empty);
    this->m_impl.doDispatch();
    ASSERT_EVENTS_StarFrameFailed_SIZE(1);
    ASSERT_TLM_StarFramesFailed_SIZE(1);
    ASSERT_EQ(this->getFrameCalls().size(), 0u);
    ASSERT_EQ(this->getReturnedFrames().size(), 0u);
}

void StarTrackerManagerTester::testCameraBusy() {
    TEST_CASE(204.3.1, "Camera busy reports execution error");
    this->clearEvents();
    this->setSubmitReturn(LionessSw::CaptureStatus::BUSY);
    this->sendCmd_TakeStarPicture(0, 0, 1280, 720);
    this->m_impl.doDispatch();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, StarTrackerManager::OPCODE_TAKESTARPICTURE, 0,
                        Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CameraBusy_SIZE(1);
}

}  // namespace Components
