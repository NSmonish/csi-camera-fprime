// StarTrackerTester.cpp
// Unit tests for the StarTracker component. The full LOST star
// identification algorithm is not integrated; these tests cover
// the delivered interface: a raw frame is accepted, validated
// and recorded, and a bad frame is rejected.
// Style rule for this project: plain punctuation only, no em dashes.

#include <gtest/gtest.h>

#include <Fw/Test/UnitTest.hpp>

#include "LionessSw/Components/StarTracker/test/ut/StarTrackerTester.hpp"
#include "LionessSw/V4L2/SimCameraDevice.hpp"

namespace Components {

StarTrackerTester::StarTrackerTester(StarTracker& inst)
    : StarTrackerGTestBase("testerbase", 100), m_impl(inst) {}

StarTrackerTester::~StarTrackerTester() {
}

void StarTrackerTester::testValidFrame() {
    TEST_CASE(205.1.1, "Valid raw frame is recorded");
    this->clearEvents();
    this->clearTlm();
    std::vector<std::uint8_t> bytes(
        CameraHal::raw10FrameBytes(1280, 720));
    CameraHal::generateRaw10Frame(bytes.data(), 1280, 720, 3);
    Fw::Buffer frame;
    frame.setData(bytes.data());
    frame.setSize(static_cast<U32>(bytes.size()));
    this->invoke_to_processFrame(0, frame, 1280, 720, 3);
    ASSERT_EVENTS_FrameReceived_SIZE(1);
    ASSERT_EVENTS_FrameReceived(0, 3, 1280, 720);
    ASSERT_TLM_FramesReceived_SIZE(1);
    ASSERT_TLM_FramesReceived(0, 1);
    ASSERT_TLM_LastFrameCounter_SIZE(1);
    ASSERT_TLM_LastFrameCounter(0, 3);
    ASSERT_EQ(this->m_impl.getFramesReceived(), 1u);
    ASSERT_EQ(this->m_impl.getLastFrameCounter(), 3u);
}

void StarTrackerTester::testInvalidFrame() {
    TEST_CASE(205.2.1, "Frame with wrong size is rejected");
    this->clearEvents();
    this->clearTlm();
    std::vector<std::uint8_t> bytes(100, 0);
    Fw::Buffer frame;
    frame.setData(bytes.data());
    frame.setSize(static_cast<U32>(bytes.size()));
    this->invoke_to_processFrame(0, frame, 1280, 720, 4);
    ASSERT_EVENTS_FrameInvalid_SIZE(1);
    ASSERT_TLM_FramesReceived_SIZE(0);
    ASSERT_EQ(this->m_impl.getFramesReceived(), 0u);
}

}  // namespace Components
