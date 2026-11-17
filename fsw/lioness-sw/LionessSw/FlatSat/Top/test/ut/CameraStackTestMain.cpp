// CameraStackTestMain.cpp
// End-to-end host test for the whole LIONESS camera stack.
// The real components are wired exactly as topology.fpp wires
// them, with the simulated devices behind the Camera component.
// Commands are sent on the real command ports, with a small
// sink for the command responses (the generated
// cmdResponse_out asserts if that port is left unconnected).
// The test checks the real storage stub files and the real
// StarTracker frame counter. No GDS and no UART are involved:
// this topology has no host command path except the UARTs, so
// the harness drives the command ports directly.
// Style rule for this project: plain punctuation only, no em dashes.

#include <Fw/Obj/SimpleObjRegistry.hpp>
#include <gtest/gtest.h>

#if defined(__SANITIZE_ADDRESS__)
#include <sanitizer/lsan_interface.h>
#endif

#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "LionessSw/Components/Camera/Camera.hpp"
#include "LionessSw/Components/ImageProcessing/ImageCodec.hpp"
#include "LionessSw/Components/ImageProcessing/ImageProcessing.hpp"
#include "LionessSw/Components/PayloadManager/PayloadManager.hpp"
#include "LionessSw/Components/StarTracker/StarTracker.hpp"
#include "LionessSw/Components/StarTrackerManager/StarTrackerManager.hpp"
#include "LionessSw/V4L2/SimCameraDevice.hpp"

// stb_image decodes the JPEG and PNG previews in the test,
// so the test checks real image content, not just magic bytes.
#define STB_IMAGE_IMPLEMENTATION
#include "LionessSw/Components/ImageProcessing/stb_image.h"

#if FW_OBJECT_REGISTRATION == 1
static Fw::SimpleObjRegistry simpleReg;
#endif

namespace {

// Minimal sink for command responses and command
// registration. The generated cmdResponse_out asserts if its
// port is left unconnected, so the managers need a sink even
// though this test never reads the responses.
class CmdSink : public Fw::PassiveComponentBase {
public:
    explicit CmdSink(const char* name) : Fw::PassiveComponentBase(name) {
        responsePort.init();
        responsePort.addCallComp(this, &CmdSink::onResponse);
        regPort.init();
        regPort.addCallComp(this, &CmdSink::onReg);
    }

    Fw::InputCmdResponsePort responsePort;
    Fw::InputCmdRegPort regPort;

    // Responses recorded so far, copied under a mutex. The
    // responses arrive on component task threads while the
    // test reads them, so the copy must be synchronized.
    std::vector<Fw::CmdResponse> responses() {
        std::lock_guard<std::mutex> lock(mutex_);
        return responses_;
    }

private:
    static void onResponse(Fw::PassiveComponentBase* comp, FwIndexType portNum,
                           FwOpcodeType opCode, U32 cmdSeq,
                           const Fw::CmdResponse& response) {
        (void)portNum;
        (void)opCode;
        (void)cmdSeq;
        CmdSink* self = static_cast<CmdSink*>(comp);
        {
            std::lock_guard<std::mutex> lock(self->mutex_);
            self->responses_.push_back(response);
        }
    }

    std::mutex mutex_;
    std::vector<Fw::CmdResponse> responses_;

    static void onReg(Fw::PassiveComponentBase* comp, FwIndexType portNum,
                      FwOpcodeType opCode) {
        (void)comp;
        (void)portNum;
        (void)opCode;
    }
};

struct Stack {
    CameraHal::SimCameraDevice payloadDevice;
    CameraHal::SimCameraDevice starDevice;
    Components::Camera camera;
    Components::ImageProcessing imageProcessing;
    Components::PayloadManager payloadManager;
    Components::StarTrackerManager starTrackerManager;
    Components::StarTracker starTracker;
    CmdSink pmSink;
    CmdSink stmSink;
    bool started_;

    Stack()
        : payloadDevice("payload"), starDevice("star"), camera("Camera"),
          imageProcessing("ImageProcessing"),
          payloadManager("PayloadManager"),
          starTrackerManager("StarTrackerManager"),
          starTracker("StarTracker"), pmSink("PmCmdSink"),
          stmSink("StmCmdSink"), started_(false) {}

    // Stop the component tasks before the members are
    // destroyed, mirroring teardownTopology in the deployment.
    ~Stack() {
        if (started_) {
            camera.exit();
            imageProcessing.exit();
            payloadManager.exit();
            starTrackerManager.exit();
            (void)camera.join();
            (void)imageProcessing.join();
            (void)payloadManager.join();
            (void)starTrackerManager.join();
            // Stop the worker threads now, while all components
            // still exist: a job in flight finishes its
            // completion calls before any member is destroyed.
            camera.shutdownWorker();
            imageProcessing.shutdownWorker();
        }
    }

    void init() {
        camera.setPayloadDevice(&payloadDevice);
        camera.setStarDevice(&starDevice);
        payloadManager.setStorageDir("e2e_storage");
        camera.init(10, 0);
        imageProcessing.init(10, 1);
        payloadManager.init(10, 2);
        starTrackerManager.init(10, 3);
        starTracker.init(4);
        // Wire the data path exactly as topology.fpp does.
        payloadManager.set_captureSubmit_OutputPort(
            0, camera.get_captureSubmitPayload_InputPort(0));
        starTrackerManager.set_captureSubmit_OutputPort(
            0, camera.get_captureSubmitStar_InputPort(0));
        camera.set_captureDonePayload_OutputPort(
            0, payloadManager.get_captureDone_InputPort(0));
        camera.set_captureDoneStar_OutputPort(
            0, starTrackerManager.get_captureDone_InputPort(0));
        camera.set_processImage_OutputPort(
            0, imageProcessing.get_processImage_InputPort(0));
        imageProcessing.set_processDone_OutputPort(
            0, camera.get_processDone_InputPort(0));
        payloadManager.set_returnOutput_OutputPort(
            0, imageProcessing.get_returnBuffer_InputPort(0));
        starTrackerManager.set_processFrame_OutputPort(
            0, starTracker.get_processFrame_InputPort(0));
        starTrackerManager.set_returnFrame_OutputPort(
            0, camera.get_returnFrame_InputPort(0));

        // Command response and registration sinks. The command
        // response port must be connected: the generated
        // cmdResponse_out asserts otherwise.
        payloadManager.set_cmdResponseOut_OutputPort(0, &pmSink.responsePort);
        payloadManager.set_cmdRegOut_OutputPort(0, &pmSink.regPort);
        starTrackerManager.set_cmdResponseOut_OutputPort(0, &stmSink.responsePort);
        starTrackerManager.set_cmdRegOut_OutputPort(0, &stmSink.regPort);
        // Log, telemetry and time ports are left unconnected.
        // The generated code guards those calls with
        // isConnected, so they are safe no-ops here.

        // Start the component tasks, as startTasks does in the
        // deployment. The tasks dispatch their own messages.
        camera.start();
        imageProcessing.start();
        payloadManager.start();
        starTrackerManager.start();
        started_ = true;
    }

    // Wait until the predicate holds or the timeout passes.
    // The component tasks do the dispatching.
    template <typename Pred>
    bool waitUntil(Pred pred, std::uint32_t timeoutMs) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - start)
                   .count() < timeoutMs) {
            if (pred()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return pred();
    }

    void sendTakePayloadPicture(U32 cmdSeq, U32 width, U32 height,
                                LionessSw::CaptureOperation operation) {
        Fw::CmdArgBuffer args;
        (void)args.serializeFrom(width);
        (void)args.serializeFrom(height);
        (void)args.serializeFrom(operation);
        payloadManager.get_cmdIn_InputPort(0)->invoke(0, cmdSeq, args);
    }

    void sendTakeStarPicture(U32 cmdSeq, U32 width, U32 height) {
        Fw::CmdArgBuffer args;
        (void)args.serializeFrom(width);
        (void)args.serializeFrom(height);
        starTrackerManager.get_cmdIn_InputPort(0)->invoke(0, cmdSeq, args);
    }
};

void clearE2eStorage() {
    std::remove("e2e_storage/payload_1_preview.jpg");
    std::remove("e2e_storage/payload_1_preview.png");
    std::remove("e2e_storage/payload_1_lossless.zlib");
    std::remove("e2e_storage/payload_2_preview.jpg");
    std::remove("e2e_storage/payload_2_preview.png");
    std::remove("e2e_storage/payload_2_lossless.zlib");
    std::remove("e2e_storage/payload_3_preview.jpg");
    std::remove("e2e_storage/payload_3_preview.png");
    std::remove("e2e_storage/payload_3_lossless.zlib");
}

}  // namespace

TEST(CameraStackTest, EndToEnd) {
    clearE2eStorage();
    // The Stack is stack allocated on purpose. F Prime v4.1.1
    // never initialized ActiveComponentBase::m_stage, so this
    // used to hit the default case assert in s_taskStateMachine
    // under ThreadSanitizer. The local patch in
    // lib/fprime/Fw/Comp/ActiveComponentBase.hpp initializes it
    // to CREATED. This test guards that patch: if it regresses,
    // this test aborts here.
    Stack stack;
    stack.init();

    // One payload picture and one star picture at the same
    // time. Each camera has its own job slot and worker, so
    // both must be accepted and both must complete. A second
    // payload picture sent at once must still be rejected as
    // Busy: the payload device holds one job at a time, so the
    // second request stores nothing and its command response
    // is EXECUTION_ERROR.
    stack.sendTakePayloadPicture(0, 1280, 720,
                                 LionessSw::CaptureOperation::BOTH);
    stack.sendTakePayloadPicture(1, 1280, 720,
                                 LionessSw::CaptureOperation::BOTH);
    stack.sendTakeStarPicture(0, 1280, 720);

    // Wait on the manager counters, not just the files: the
    // counters are incremented last in the completion handlers,
    // so they prove the handlers finished.
    ASSERT_TRUE(stack.waitUntil(
        [&stack] {
            return stack.payloadManager.getPicturesTaken() == 1 &&
                   stack.starTrackerManager.getStarFramesTaken() == 1 &&
                   stack.payloadManager.storage().exists(
                       "payload_1_preview.jpg") &&
                   stack.payloadManager.storage().exists(
                       "payload_1_preview.png") &&
                   stack.payloadManager.storage().exists(
                       "payload_1_lossless.zlib");
        },
        15000));

    // Exactly one payload picture was stored, and the Busy
    // second request stored nothing.
    ASSERT_EQ(stack.payloadManager.getPicturesTaken(), 1u);
    ASSERT_FALSE(stack.payloadManager.storage().exists(
        "payload_2_preview.jpg"));
    ASSERT_FALSE(stack.payloadManager.storage().exists(
        "payload_2_preview.png"));
    ASSERT_FALSE(stack.payloadManager.storage().exists(
        "payload_2_lossless.zlib"));
    {
        std::vector<Fw::CmdResponse> responses = stack.pmSink.responses();
        ASSERT_EQ(responses.size(), 2u);
        ASSERT_EQ(responses[0], Fw::CmdResponse::OK);
        ASSERT_EQ(responses[1], Fw::CmdResponse::EXECUTION_ERROR);
    }

    // The star picture completed at the same time as the
    // payload picture, on its own device.
    ASSERT_EQ(stack.starTrackerManager.getStarFramesTaken(), 1u);
    ASSERT_EQ(stack.starTracker.getLastFrameCounter(), 1u);
    {
        std::vector<Fw::CmdResponse> responses = stack.stmSink.responses();
        ASSERT_EQ(responses.size(), 1u);
        ASSERT_EQ(responses[0], Fw::CmdResponse::OK);
    }

    // The previews are real JPEG and PNG, both encoded by stb
    // from the same downsampled frame. Check the magic bytes,
    // decode both, and check the pixels match: the PNG holds
    // the exact downsampled pixels, the JPEG is lossy, so its
    // pixels must only be close.
    {
        std::vector<std::uint8_t> preview(200000);
        std::size_t size = 0;
        ASSERT_TRUE(stack.payloadManager.storage().readFile(
            "payload_1_preview.jpg", preview.data(), preview.size(), size));
        ASSERT_GT(size, 0u);
        ASSERT_TRUE(ImageCodec::previewIsJpeg());
        ASSERT_EQ(preview[0], 0xFF);
        ASSERT_EQ(preview[1], 0xD8);
        std::vector<std::uint8_t> png(200000);
        std::size_t pngSize = 0;
        ASSERT_TRUE(stack.payloadManager.storage().readFile(
            "payload_1_preview.png", png.data(), png.size(), pngSize));
        ASSERT_GT(pngSize, 0u);
        ASSERT_EQ(png[0], 0x89);
        ASSERT_EQ(png[1], 0x50);
        ASSERT_EQ(png[2], 0x4E);
        ASSERT_EQ(png[3], 0x47);
        int jpgW = 0, jpgH = 0, jpgC = 0;
        unsigned char* jpgPixels = stbi_load_from_memory(
            preview.data(), static_cast<int>(size), &jpgW, &jpgH, &jpgC, 1);
        ASSERT_TRUE(jpgPixels != nullptr);
        ASSERT_EQ(jpgW, 320);
        ASSERT_EQ(jpgH, 240);
        int pngW = 0, pngH = 0, pngC = 0;
        unsigned char* pngPixels = stbi_load_from_memory(
            png.data(), static_cast<int>(pngSize), &pngW, &pngH, &pngC, 1);
        ASSERT_TRUE(pngPixels != nullptr);
        ASSERT_EQ(pngW, 320);
        ASSERT_EQ(pngH, 240);
        // The JPEG is lossy at quality 30, so its pixels must
        // only be close on average: the sampled mean difference
        // must be at most 8 grey levels, and at least 90 percent
        // of sampled pixels must be within 16 grey levels.
        // Measured on this gradient frame: mean 4.6, 94 percent
        // within 16, worst 98 at the sharp sawtooth edges, where
        // JPEG ringing is worst. A different frame would fail
        // both bounds by a wide margin.
        {
            long totalDiff = 0;
            long samples = 0;
            long close = 0;
            for (int y = 0; y < 240; y += 17) {
                for (int x = 0; x < 320; x += 19) {
                    int i = y * 320 + x;
                    int diff = static_cast<int>(jpgPixels[i]) -
                               static_cast<int>(pngPixels[i]);
                    if (diff < 0) diff = -diff;
                    totalDiff += diff;
                    ++samples;
                    if (diff <= 16) ++close;
                }
            }
            ASSERT_LE(totalDiff / samples, 8);
            ASSERT_GE(close * 100 / samples, 90);
        }
        stbi_image_free(jpgPixels);
        stbi_image_free(pngPixels);
    }

    // The lossless product decompresses byte for byte to the
    // frame the simulated camera generated (counter 1).
    {
        std::vector<std::uint8_t> expected(
            CameraHal::raw10FrameBytes(1280, 720));
        CameraHal::generateRaw10Frame(expected.data(), 1280, 720, 1);
        std::vector<std::uint8_t> compressed(2000000);
        std::size_t compressedSize = 0;
        ASSERT_TRUE(stack.payloadManager.storage().readFile(
            "payload_1_lossless.zlib", compressed.data(), compressed.size(),
            compressedSize));
        ASSERT_GT(compressedSize, 0u);
        std::vector<std::uint8_t> decoded(expected.size());
        std::size_t decodedBytes = 0;
        ASSERT_TRUE(ImageCodec::decompressLossless(
            compressed.data(), compressedSize, decoded.data(), decoded.size(),
            decodedBytes));
        ASSERT_EQ(decodedBytes, expected.size());
        ASSERT_EQ(std::memcmp(decoded.data(), expected.data(),
                              expected.size()),
                  0);
    }

    // A star capture that times out delivers no frame and
    // leaves the stack usable.
    stack.starDevice.setFailTimeout(true);
    stack.sendTakeStarPicture(1, 1280, 720);
    ASSERT_TRUE(stack.waitUntil(
        [&stack] {
            return stack.starTrackerManager.getStarFramesFailed() == 1;
        },
        10000));
    ASSERT_EQ(stack.starTracker.getFramesReceived(), 1u);
    stack.starDevice.setFailTimeout(false);

    // The next star capture works, and its frame counter is 2.
    stack.sendTakeStarPicture(2, 1280, 720);
    ASSERT_TRUE(stack.waitUntil(
        [&stack] {
            return stack.starTrackerManager.getStarFramesTaken() == 2;
        },
        10000));
    ASSERT_EQ(stack.starTracker.getLastFrameCounter(), 2u);
    ASSERT_EQ(stack.starTrackerManager.getStarFramesTaken(), 2u);

    // A second payload picture after the first completed proves
    // the output buffers were returned and reused.
    stack.sendTakePayloadPicture(2, 1280, 720,
                                 LionessSw::CaptureOperation::BOTH);
    ASSERT_TRUE(stack.waitUntil(
        [&stack] {
            return stack.payloadManager.getPicturesTaken() == 2 &&
                   stack.payloadManager.storage().exists(
                       "payload_3_preview.jpg") &&
                   stack.payloadManager.storage().exists(
                       "payload_3_preview.png") &&
                   stack.payloadManager.storage().exists(
                       "payload_3_lossless.zlib");
        },
        15000));
    ASSERT_EQ(stack.payloadManager.getPicturesTaken(), 2u);
    clearE2eStorage();
}

int main(int argc, char* argv[]) {
#if defined(__SANITIZE_ADDRESS__)
    // Leak checking is off for this executable only. The four
    // component message queues are freed by
    // QueuedComponentBase::deinit, which is protected: only the
    // per-component Tester friends can call it, and this
    // harness is not one. The queues are the framework's
    // process-lifetime allocations; the unit tests call deinit
    // and are leak-clean, which is where leaks are checked.
    __lsan_disable();
#endif
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
