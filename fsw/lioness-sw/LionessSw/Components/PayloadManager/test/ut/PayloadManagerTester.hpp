// PayloadManagerTester.hpp
// Unit test harness for the PayloadManager component. The tester
// plays the Camera component (from_captureSubmit) and
// ImageProcessing (from_returnOutput).
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef LionessSw_Components_PayloadManager_test_ut_PayloadManagerTester_HPP
#define LionessSw_Components_PayloadManager_test_ut_PayloadManagerTester_HPP

#include <cstdint>
#include <vector>

#include "PayloadManagerGTestBase.hpp"
#include "LionessSw/Components/PayloadManager/PayloadManager.hpp"

namespace Components {

class PayloadManagerTester : public PayloadManagerGTestBase {
public:
    explicit PayloadManagerTester(PayloadManager& inst);
    ~PayloadManagerTester();

    void testTakePictureStoresProduct();
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
    std::vector<Fw::Buffer> getReturnedBuffers() const {
        return m_returnedBuffers;
    }

    void setSubmitReturn(LionessSw::CaptureStatus status) {
        m_submitReturn = status;
    }

private:
    LionessSw::CaptureStatus from_captureSubmit_handler(
        FwIndexType portNum, std::uint32_t requestId,
        const LionessSw::CameraId& cameraId, std::uint32_t width,
        std::uint32_t height, const LionessSw::CaptureOperation& operation,
        std::uint32_t timeoutMs) override;

    void from_returnOutput_handler(FwIndexType portNum,
                                   const Fw::Buffer& buffer) override;

    PayloadManager& m_impl;
    std::vector<SubmitCall> m_submitCalls;
    std::vector<Fw::Buffer> m_returnedBuffers;
    LionessSw::CaptureStatus m_submitReturn;
};

}  // namespace Components

#endif
