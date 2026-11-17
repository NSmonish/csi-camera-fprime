// StarTrackerTestMain.cpp
// Unit test driver for the StarTracker component.
// Style rule for this project: plain punctuation only, no em dashes.

#include <Fw/Obj/SimpleObjRegistry.hpp>
#include <gtest/gtest.h>

#include "LionessSw/Components/StarTracker/StarTracker.hpp"
#include "LionessSw/Components/StarTracker/test/ut/StarTrackerTester.hpp"

#if FW_OBJECT_REGISTRATION == 1
static Fw::SimpleObjRegistry simpleReg;
#endif

namespace Components {

void connectPorts(StarTracker& impl, StarTrackerTester& tester) {
    tester.connect_to_processFrame(0, impl.get_processFrame_InputPort(0));
    impl.set_logOut_OutputPort(0, tester.get_from_logOut(0));
    impl.set_logTextOut_OutputPort(0, tester.get_from_logTextOut(0));
    impl.set_tlmOut_OutputPort(0, tester.get_from_tlmOut(0));
    impl.set_timeCaller_OutputPort(0, tester.get_from_timeCaller(0));
}

TEST(StarTrackerTest, ValidFrame) {
    StarTracker impl("StarTracker");
    StarTrackerTester tester(impl);
    tester.init();
    impl.init(0);
    connectPorts(impl, tester);
    tester.testValidFrame();
}

TEST(StarTrackerTest, InvalidFrame) {
    StarTracker impl("StarTracker");
    StarTrackerTester tester(impl);
    tester.init();
    impl.init(0);
    connectPorts(impl, tester);
    tester.testInvalidFrame();
}

}  // namespace Components

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
