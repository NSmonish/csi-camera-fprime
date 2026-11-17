// StarTrackerManagerTestMain.cpp
// Unit test driver for the StarTrackerManager component.
// Style rule for this project: plain punctuation only, no em dashes.

#include <Fw/Obj/SimpleObjRegistry.hpp>
#include <gtest/gtest.h>

#include "LionessSw/Components/StarTrackerManager/StarTrackerManager.hpp"
#include "LionessSw/Components/StarTrackerManager/test/ut/StarTrackerManagerTester.hpp"

#if FW_OBJECT_REGISTRATION == 1
static Fw::SimpleObjRegistry simpleReg;
#endif

namespace Components {

void connectPorts(StarTrackerManager& impl, StarTrackerManagerTester& tester) {
    impl.set_captureSubmit_OutputPort(0, tester.get_from_captureSubmit(0));
    impl.set_returnFrame_OutputPort(0, tester.get_from_returnFrame(0));
    impl.set_processFrame_OutputPort(0, tester.get_from_processFrame(0));
    tester.connect_to_captureDone(0, impl.get_captureDone_InputPort(0));
    impl.set_logOut_OutputPort(0, tester.get_from_logOut(0));
    impl.set_logTextOut_OutputPort(0, tester.get_from_logTextOut(0));
    impl.set_tlmOut_OutputPort(0, tester.get_from_tlmOut(0));
    impl.set_timeCaller_OutputPort(0, tester.get_from_timeCaller(0));
    impl.set_cmdRegOut_OutputPort(0, tester.get_from_cmdRegOut(0));
    impl.set_cmdResponseOut_OutputPort(0, tester.get_from_cmdResponseOut(0));
    tester.connect_to_cmdIn(0, impl.get_cmdIn_InputPort(0));
}

TEST(StarTrackerManagerTest, TakeStarPicture) {
    StarTrackerManager impl("StarTrackerManager");
    StarTrackerManagerTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testTakeStarPicture();
}

TEST(StarTrackerManagerTest, CaptureFailed) {
    StarTrackerManager impl("StarTrackerManager");
    StarTrackerManagerTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testCaptureFailed();
}

TEST(StarTrackerManagerTest, CameraBusy) {
    StarTrackerManager impl("StarTrackerManager");
    StarTrackerManagerTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testCameraBusy();
}

}  // namespace Components

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
