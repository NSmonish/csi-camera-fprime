// PayloadManagerTestMain.cpp
// Unit test driver for the PayloadManager component.
// Style rule for this project: plain punctuation only, no em dashes.

#include <Fw/Obj/SimpleObjRegistry.hpp>
#include <gtest/gtest.h>

#include <cstdio>

#include "LionessSw/Components/PayloadManager/PayloadManager.hpp"
#include "LionessSw/Components/PayloadManager/test/ut/PayloadManagerTester.hpp"

#if FW_OBJECT_REGISTRATION == 1
static Fw::SimpleObjRegistry simpleReg;
#endif

namespace Components {

void connectPorts(PayloadManager& impl, PayloadManagerTester& tester) {
    impl.set_captureSubmit_OutputPort(0, tester.get_from_captureSubmit(0));
    impl.set_returnOutput_OutputPort(0, tester.get_from_returnOutput(0));
    tester.connect_to_captureDone(0, impl.get_captureDone_InputPort(0));
    impl.set_logOut_OutputPort(0, tester.get_from_logOut(0));
    impl.set_logTextOut_OutputPort(0, tester.get_from_logTextOut(0));
    impl.set_tlmOut_OutputPort(0, tester.get_from_tlmOut(0));
    impl.set_timeCaller_OutputPort(0, tester.get_from_timeCaller(0));
    impl.set_cmdRegOut_OutputPort(0, tester.get_from_cmdRegOut(0));
    impl.set_cmdResponseOut_OutputPort(0, tester.get_from_cmdResponseOut(0));
    tester.connect_to_cmdIn(0, impl.get_cmdIn_InputPort(0));
}

TEST(PayloadManagerTest, TakePictureStoresProduct) {
    PayloadManager impl("PayloadManager");
    impl.setStorageDir("test_storage_payload");
    // Clear the three product files from any earlier run.
    std::remove("test_storage_payload/payload_1_preview.jpg");
    std::remove("test_storage_payload/payload_1_preview.png");
    std::remove("test_storage_payload/payload_1_lossless.zlib");
    PayloadManagerTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testTakePictureStoresProduct();
    // Clear the three product files from any earlier run.
    std::remove("test_storage_payload/payload_1_preview.jpg");
    std::remove("test_storage_payload/payload_1_preview.png");
    std::remove("test_storage_payload/payload_1_lossless.zlib");
}

TEST(PayloadManagerTest, CaptureFailed) {
    PayloadManager impl("PayloadManager");
    impl.setStorageDir("test_storage_payload");
    // Clear the three product files from any earlier run.
    std::remove("test_storage_payload/payload_1_preview.jpg");
    std::remove("test_storage_payload/payload_1_preview.png");
    std::remove("test_storage_payload/payload_1_lossless.zlib");
    PayloadManagerTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testCaptureFailed();
    // Clear the three product files from any earlier run.
    std::remove("test_storage_payload/payload_1_preview.jpg");
    std::remove("test_storage_payload/payload_1_preview.png");
    std::remove("test_storage_payload/payload_1_lossless.zlib");
}

TEST(PayloadManagerTest, CameraBusy) {
    PayloadManager impl("PayloadManager");
    impl.setStorageDir("test_storage_payload");
    // Clear the three product files from any earlier run.
    std::remove("test_storage_payload/payload_1_preview.jpg");
    std::remove("test_storage_payload/payload_1_preview.png");
    std::remove("test_storage_payload/payload_1_lossless.zlib");
    PayloadManagerTester tester(impl);
    tester.init();
    impl.init(10, 0);
    connectPorts(impl, tester);
    tester.testCameraBusy();
    // Clear the three product files from any earlier run.
    std::remove("test_storage_payload/payload_1_preview.jpg");
    std::remove("test_storage_payload/payload_1_preview.png");
    std::remove("test_storage_payload/payload_1_lossless.zlib");
}

}  // namespace Components

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
