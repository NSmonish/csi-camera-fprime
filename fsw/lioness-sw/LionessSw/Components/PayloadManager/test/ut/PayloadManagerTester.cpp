// PayloadManagerTester.cpp
// Unit tests for the PayloadManager component.
// Style rule for this project: plain punctuation only, no em dashes.

#include <gtest/gtest.h>

#include <cstring>

#include <Fw/Test/UnitTest.hpp>

#include "LionessSw/Components/PayloadManager/test/ut/PayloadManagerTester.hpp"

namespace Components {

PayloadManagerTester::PayloadManagerTester(PayloadManager& inst)
    : PayloadManagerGTestBase("testerbase", 100), m_impl(inst),
      m_submitReturn(LionessSw::CaptureStatus::OK) {}

PayloadManagerTester::~PayloadManagerTester() {
    this->m_impl.deinit();
}

LionessSw::CaptureStatus PayloadManagerTester::from_captureSubmit_handler(
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

void PayloadManagerTester::from_returnOutput_handler(
    FwIndexType portNum, const Fw::Buffer& buffer) {
    (void)portNum;
    this->pushFromPortEntry_returnOutput(buffer);
    m_returnedBuffers.push_back(buffer);
}

namespace {

// Build a fake completion with the given product bytes.
struct FakeProducts {
    std::vector<std::uint8_t> preview;
    std::vector<std::uint8_t> lossless;
    std::vector<std::uint8_t> png;
    Fw::Buffer previewBuf;
    Fw::Buffer losslessBuf;
    Fw::Buffer pngBuf;

    FakeProducts() {
        preview.assign(100, 0x11);
        preview[0] = 0xFF;
        preview[1] = 0xD8;
        lossless.assign(200, 0x22);
        png.assign(150, 0x33);
        png[0] = 0x89;
        png[1] = 0x50;
        png[2] = 0x4E;
        png[3] = 0x47;
        previewBuf.setData(preview.data());
        previewBuf.setSize(static_cast<U32>(preview.size()));
        losslessBuf.setData(lossless.data());
        losslessBuf.setSize(static_cast<U32>(lossless.size()));
        pngBuf.setData(png.data());
        pngBuf.setSize(static_cast<U32>(png.size()));
    }
};

}  // namespace

void PayloadManagerTester::testTakePictureStoresProduct() {
    TEST_CASE(203.1.1, "Take picture stores preview and lossless files");
    this->clearEvents();
    this->clearTlm();
    this->sendCmd_TakePayloadPicture(0, 0, 1280, 720,
                                     LionessSw::CaptureOperation::BOTH);
    this->m_impl.doDispatch();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_EQ(this->getSubmitCalls().size(), 1u);
    ASSERT_EQ(this->getSubmitCalls()[0].cameraId,
              LionessSw::CameraId::PAYLOAD);
    ASSERT_EQ(this->getSubmitCalls()[0].width, 1280u);
    ASSERT_EQ(this->getSubmitCalls()[0].height, 720u);

    FakeProducts products;
    Fw::Buffer empty;
    this->invoke_to_captureDone(0, 1, LionessSw::CaptureStatus::OK,
                                LionessSw::CameraId::PAYLOAD, 1280, 720,
                                1152000, 1, 100, 200, 150, 999,
                                products.previewBuf, products.losslessBuf,
                                products.pngBuf, empty);
    this->m_impl.doDispatch();
    ASSERT_EVENTS_PictureStored_SIZE(1);
    ASSERT_TLM_PicturesTaken_SIZE(1);
    ASSERT_TLM_BytesStored_SIZE(1);
    // All three products were written to the storage stub.
    ASSERT_TRUE(this->m_impl.storage().exists("payload_1_preview.jpg"));
    ASSERT_TRUE(this->m_impl.storage().exists("payload_1_preview.png"));
    ASSERT_TRUE(this->m_impl.storage().exists("payload_1_lossless.zlib"));
    std::vector<std::uint8_t> back(200);
    std::size_t size = 0;
    ASSERT_TRUE(this->m_impl.storage().readFile("payload_1_lossless.zlib",
                                                back.data(), back.size(), size));
    ASSERT_EQ(size, 200u);
    ASSERT_EQ(std::memcmp(back.data(), products.lossless.data(), 200), 0);
    std::vector<std::uint8_t> backPng(150);
    ASSERT_TRUE(this->m_impl.storage().readFile("payload_1_preview.png",
                                                backPng.data(),
                                                backPng.size(), size));
    ASSERT_EQ(size, 150u);
    ASSERT_EQ(std::memcmp(backPng.data(), products.png.data(), 150), 0);
    // All three buffers were returned to ImageProcessing.
    ASSERT_EQ(this->getReturnedBuffers().size(), 3u);
}

void PayloadManagerTester::testCaptureFailed() {
    TEST_CASE(203.2.1, "Failed capture stores nothing");
    this->clearEvents();
    this->clearTlm();
    this->sendCmd_TakePayloadPicture(0, 0, 1280, 720,
                                     LionessSw::CaptureOperation::BOTH);
    this->m_impl.doDispatch();
    Fw::Buffer empty;
    this->invoke_to_captureDone(0, 1, LionessSw::CaptureStatus::TIMEOUT,
                                LionessSw::CameraId::PAYLOAD, 1280, 720, 0, 0,
                                0, 0, 0, 0, empty, empty, empty, empty);
    this->m_impl.doDispatch();
    ASSERT_EVENTS_PictureFailed_SIZE(1);
    ASSERT_TLM_PicturesFailed_SIZE(1);
    ASSERT_FALSE(this->m_impl.storage().exists("payload_1_preview.jpg"));
    ASSERT_EQ(this->getReturnedBuffers().size(), 0u);
}

void PayloadManagerTester::testCameraBusy() {
    TEST_CASE(203.3.1, "Camera busy reports execution error");
    this->clearEvents();
    this->setSubmitReturn(LionessSw::CaptureStatus::BUSY);
    this->sendCmd_TakePayloadPicture(0, 0, 1280, 720,
                                     LionessSw::CaptureOperation::BOTH);
    this->m_impl.doDispatch();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, PayloadManager::OPCODE_TAKEPAYLOADPICTURE, 0,
                        Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CameraBusy_SIZE(1);
}

}  // namespace Components
