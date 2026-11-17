// CameraTestMain.cpp
// Unit test driver for the Camera component.
// Style rule for this project: plain punctuation only, no em dashes.

#include <Fw/Obj/SimpleObjRegistry.hpp>
#include <gtest/gtest.h>

#include "LionessSw/Components/Camera/Camera.hpp"
#include "LionessSw/Components/Camera/test/ut/CameraTester.hpp"
#include "LionessSw/V4L2/SimCameraDevice.hpp"

#if FW_OBJECT_REGISTRATION == 1
static Fw::SimpleObjRegistry simpleReg;
#endif

namespace Components {

void connectPorts(Camera& impl, CameraTester& tester) {
    tester.connect_to_captureSubmitPayload(
        0, impl.get_captureSubmitPayload_InputPort(0));
    tester.connect_to_captureSubmitStar(
        0, impl.get_captureSubmitStar_InputPort(0));
    tester.connect_to_processDone(0, impl.get_processDone_InputPort(0));
    tester.connect_to_returnFrame(0, impl.get_returnFrame_InputPort(0));
    impl.set_captureDonePayload_OutputPort(
        0, tester.get_from_captureDonePayload(0));
    impl.set_captureDoneStar_OutputPort(
        0, tester.get_from_captureDoneStar(0));
    impl.set_processImage_OutputPort(0, tester.get_from_processImage(0));
    impl.set_logOut_OutputPort(0, tester.get_from_logOut(0));
    impl.set_logTextOut_OutputPort(0, tester.get_from_logTextOut(0));
    impl.set_tlmOut_OutputPort(0, tester.get_from_tlmOut(0));
    impl.set_timeCaller_OutputPort(0, tester.get_from_timeCaller(0));
    impl.set_cmdRegOut_OutputPort(0, tester.get_from_cmdRegOut(0));
    impl.set_cmdResponseOut_OutputPort(0, tester.get_from_cmdResponseOut(0));
    tester.connect_to_cmdIn(0, impl.get_cmdIn_InputPort(0));
}

TEST(CameraTest, PayloadBoth) {
    CameraHal::SimCameraDevice payload("payload");
    CameraHal::SimCameraDevice star("star");
    Camera impl("Camera");
    impl.setPayloadDevice(&payload);
    impl.setStarDevice(&star);
    CameraTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testPayloadBoth();
}

TEST(CameraTest, StarRaw) {
    CameraHal::SimCameraDevice payload("payload");
    CameraHal::SimCameraDevice star("star");
    Camera impl("Camera");
    impl.setPayloadDevice(&payload);
    impl.setStarDevice(&star);
    CameraTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testStarRaw();
}

TEST(CameraTest, Timeout) {
    CameraHal::SimCameraDevice payload("payload");
    CameraHal::SimCameraDevice star("star");
    Camera impl("Camera");
    impl.setPayloadDevice(&payload);
    impl.setStarDevice(&star);
    CameraTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testTimeout();
}

TEST(CameraTest, Busy) {
    CameraHal::SimCameraDevice payload("payload");
    CameraHal::SimCameraDevice star("star");
    Camera impl("Camera");
    impl.setPayloadDevice(&payload);
    impl.setStarDevice(&star);
    CameraTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testBusy();
}

TEST(CameraTest, Invalid) {
    CameraHal::SimCameraDevice payload("payload");
    CameraHal::SimCameraDevice star("star");
    Camera impl("Camera");
    impl.setPayloadDevice(&payload);
    impl.setStarDevice(&star);
    CameraTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testInvalid();
}

TEST(CameraTest, TakePictureCommand) {
    CameraHal::SimCameraDevice payload("payload");
    CameraHal::SimCameraDevice star("star");
    Camera impl("Camera");
    impl.setPayloadDevice(&payload);
    impl.setStarDevice(&star);
    CameraTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testTakePictureCommand();
}

TEST(CameraTest, ConcurrentBoth) {
    CameraHal::SimCameraDevice payload("payload");
    CameraHal::SimCameraDevice star("star");
    Camera impl("Camera");
    impl.setPayloadDevice(&payload);
    impl.setStarDevice(&star);
    CameraTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testConcurrentBoth();
}

}  // namespace Components

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
