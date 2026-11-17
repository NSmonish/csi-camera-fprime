// ImageProcessingTestMain.cpp
// Unit test driver for the ImageProcessing component.
// Style rule for this project: plain punctuation only, no em dashes.

#include <Fw/Obj/SimpleObjRegistry.hpp>
#include <gtest/gtest.h>

#include "LionessSw/Components/ImageProcessing/ImageProcessing.hpp"
#include "LionessSw/Components/ImageProcessing/test/ut/ImageProcessingTester.hpp"

#if FW_OBJECT_REGISTRATION == 1
static Fw::SimpleObjRegistry simpleReg;
#endif

namespace Components {

void connectPorts(ImageProcessing& impl, ImageProcessingTester& tester) {
    tester.connect_to_processImage(0, impl.get_processImage_InputPort(0));
    tester.connect_to_returnBuffer(0, impl.get_returnBuffer_InputPort(0));
    impl.set_processDone_OutputPort(0, tester.get_from_processDone(0));
    impl.set_logOut_OutputPort(0, tester.get_from_logOut(0));
    impl.set_logTextOut_OutputPort(0, tester.get_from_logTextOut(0));
    impl.set_tlmOut_OutputPort(0, tester.get_from_tlmOut(0));
    impl.set_timeCaller_OutputPort(0, tester.get_from_timeCaller(0));
    impl.set_cmdRegOut_OutputPort(0, tester.get_from_cmdRegOut(0));
    impl.set_cmdResponseOut_OutputPort(0, tester.get_from_cmdResponseOut(0));
    tester.connect_to_cmdIn(0, impl.get_cmdIn_InputPort(0));
}

TEST(ImageProcessingTest, PreviewAndLossless) {
    ImageProcessing impl("ImageProcessing");
    ImageProcessingTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testPreviewAndLossless();
}

TEST(ImageProcessingTest, Busy) {
    ImageProcessing impl("ImageProcessing");
    ImageProcessingTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testBusy();
}

TEST(ImageProcessingTest, Invalid) {
    ImageProcessing impl("ImageProcessing");
    ImageProcessingTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testInvalid();
}

TEST(ImageProcessingTest, PreviewOnly) {
    ImageProcessing impl("ImageProcessing");
    ImageProcessingTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testPreviewOnly();
}

TEST(ImageProcessingTest, LosslessOnly) {
    ImageProcessing impl("ImageProcessing");
    ImageProcessingTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testLosslessOnly();
}

}  // namespace Components

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
