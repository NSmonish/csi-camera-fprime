// StarTrackerManagerTester.hpp
// Unit test harness for the StarTrackerManager component. The
// tester plays the Camera component and the StarTracker
// component.
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef LionessSw_Components_StarTrackerManager_test_ut_StarTrackerManagerTester_HPP
#define LionessSw_Components_StarTrackerManager_test_ut_StarTrackerManagerTester_HPP

#include <cstdint>
#include <vector>

#include "StarTrackerManagerGTestBase.hpp"
#include "LionessSw/Components/StarTrackerManager/StarTrackerManager.hpp"

namespace Components {

class StarTrackerManagerTester : public StarTrackerManagerGTestBase {
public:
    explicit StarTrackerManagerTester(StarTrackerManager& inst);
    ~StarTrackerManagerTester();

    void testTakeStarPicture();
    void testCaptureFailed();
    void testCameraBusy();

    struct SubmitCall {
        std::uint32_t requestId;
        LionessSw::CameraId cameraId;
        std::uint32_t width;
        std::uint32_t height;
        LionessSw::CaptureOperation operation;
        std::uint32_t timeoutMs;
    };

    std::vector<SubmitCall> getSubmitCalls() const { return m_submitCalls; }
    std::vector<Fw::Buffer> getReturnedFrames() const {
        return m_returnedFrames;
    }

    struct FrameCall {
        Fw::Buffer frame;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t frameCounter;
    };
    std::vector<FrameCall> getFrameCalls() const { return m_frameCalls; }

    void setSubmitReturn(LionessSw::CaptureStatus status) {
        m_submitReturn = status;
    }

private:
    LionessSw::CaptureStatus from_captureSubmit_handler(
        FwIndexType portNum, std::uint32_t requestId,
        const LionessSw::CameraId& cameraId, std::uint32_t width,
        std::uint32_t height, const LionessSw::CaptureOperation& operation,
        std::uint32_t timeoutMs) override;

    void from_returnFrame_handler(FwIndexType portNum,
                                  const Fw::Buffer& buffer) override;

    void from_processFrame_handler(FwIndexType portNum,
                                   const Fw::Buffer& frame, std::uint32_t width,
                                   std::uint32_t height,
                                   std::uint32_t frameCounter) override;

    StarTrackerManager& m_impl;
    std::vector<SubmitCall> m_submitCalls;
    std::vector<Fw::Buffer> m_returnedFrames;
    std::vector<FrameCall> m_frameCalls;
    LionessSw::CaptureStatus m_submitReturn;
};

}  // namespace Components

#endif
