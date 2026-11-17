// SimCameraDevice.cpp
// Simulated camera device. See the header for the design.
// Adapted from the verified simulation in
// workspace/csi-camera-capture/src/simulated_camera.cpp, with the
// same frame pattern and counter markers, so the host tests here
// check the same bytes the earlier simulation proved.
// Style rule for this project: plain punctuation only, no em dashes.

#include "SimCameraDevice.hpp"

#include <cerrno>
#include <chrono>
#include <thread>

#include "videodev2_compat.h"

namespace CameraHal {

std::uint16_t unpackRaw10Pixel(const std::uint8_t* packed, std::size_t pixelIndex) {
    std::size_t group = pixelIndex / 4;
    std::size_t slot = pixelIndex % 4;
    const std::uint8_t* b = packed + group * 5;
    switch (slot) {
        case 0:
            return static_cast<std::uint16_t>((b[0] << 2) | (b[1] >> 6));
        case 1:
            return static_cast<std::uint16_t>(((b[1] & 0x3F) << 4) | (b[2] >> 4));
        case 2:
            return static_cast<std::uint16_t>(((b[2] & 0x0F) << 6) | (b[3] >> 2));
        default:
            return static_cast<std::uint16_t>(((b[3] & 0x03) << 8) | b[4]);
    }
}

static void packRaw10Pixel(std::uint8_t* packed, std::size_t pixelIndex,
                           std::uint16_t value) {
    std::size_t group = pixelIndex / 4;
    std::size_t slot = pixelIndex % 4;
    std::uint8_t* b = packed + group * 5;
    switch (slot) {
        case 0:
            b[0] = static_cast<std::uint8_t>(value >> 2);
            b[1] = static_cast<std::uint8_t>((b[1] & 0x3F) | ((value & 3) << 6));
            break;
        case 1:
            b[1] = static_cast<std::uint8_t>((b[1] & 0xC0) | (value >> 4));
            b[2] = static_cast<std::uint8_t>((b[2] & 0x0F) | ((value & 15) << 4));
            break;
        case 2:
            b[2] = static_cast<std::uint8_t>((b[2] & 0xF0) | (value >> 6));
            b[3] = static_cast<std::uint8_t>((b[3] & 0x03) | ((value & 63) << 2));
            break;
        default:
            b[3] = static_cast<std::uint8_t>((b[3] & 0xFC) | (value >> 8));
            b[4] = static_cast<std::uint8_t>(value & 0xFF);
            break;
    }
}

void generateRaw10Frame(std::uint8_t* dest, std::uint32_t width,
                        std::uint32_t height, std::uint32_t frameCounter) {
    std::size_t totalBytes = raw10FrameBytes(width, height);
    for (std::size_t i = 0; i < totalBytes; ++i) dest[i] = 0;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::size_t idx =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + x;
            std::uint16_t value = static_cast<std::uint16_t>(
                (3u * x + 5u * y + 11u * frameCounter) & 0x3FFu);
            packRaw10Pixel(dest, idx, value);
        }
    }
    // Frame counter in a known location: first four pixels of row zero.
    packRaw10Pixel(dest, 0, static_cast<std::uint16_t>(frameCounter & 0x3FFu));
    packRaw10Pixel(dest, 1,
                   static_cast<std::uint16_t>((frameCounter >> 10) & 0x3FFu));
    packRaw10Pixel(dest, 2, 0x3FF);
    packRaw10Pixel(dest, 3, 0x2AA);
}

std::uint32_t readFrameCounter(const std::uint8_t* frame, std::uint32_t width,
                               std::uint32_t height) {
    (void)width;
    (void)height;
    std::uint32_t low = unpackRaw10Pixel(frame, 0);
    std::uint32_t high = unpackRaw10Pixel(frame, 1);
    return low | (high << 10);
}

SimCameraDevice::SimCameraDevice(const std::string& label)
    : label_(label), open_(false), streaming_(false), width_(1280),
      height_(720), pixelFormat_(V4L2_PIX_FMT_SRGGB10), latencyMs_(120),
      failTimeout_(false), frameCounter_(0), capturing_(false), nextRing_(0) {}

int SimCameraDevice::open(const std::string& devicePath) {
    (void)devicePath;
    if (open_) return -EBUSY;
    open_ = true;
    return 0;
}

void SimCameraDevice::close() {
    streaming_ = false;
    capturing_ = false;
    open_ = false;
    ring_.clear();
    ringInUse_.clear();
}

int SimCameraDevice::setFormat(std::uint32_t width, std::uint32_t height,
                               std::uint32_t pixelFormat) {
    if (!open_) return -ENODEV;
    if (pixelFormat != V4L2_PIX_FMT_SRGGB10) return -EINVAL;
    if (!isSupportedMode(width, height)) return -EINVAL;
    width_ = width;
    height_ = height;
    pixelFormat_ = pixelFormat;
    return 0;
}

void SimCameraDevice::getFormat(std::uint32_t& width, std::uint32_t& height,
                                std::uint32_t& pixelFormat) const {
    width = width_;
    height = height_;
    pixelFormat = pixelFormat_;
}

int SimCameraDevice::allocateBuffers(std::uint32_t count) {
    if (!open_) return -ENODEV;
    if (count == 0 || count > 8) return -EINVAL;
    std::size_t bytes = raw10FrameBytes(width_, height_);
    ring_.assign(count, std::vector<std::uint8_t>(bytes));
    ringInUse_.assign(count, false);
    nextRing_ = 0;
    return 0;
}

int SimCameraDevice::startStreaming() {
    if (!open_) return -ENODEV;
    if (ring_.empty()) return -ENOMEM;
    streaming_ = true;
    return 0;
}

int SimCameraDevice::stopStreaming() {
    streaming_ = false;
    capturing_ = false;
    return 0;
}

bool SimCameraDevice::frameReady() const {
    if (!capturing_ || failTimeout_) return false;
    std::chrono::steady_clock::time_point ready =
        captureStart_ + std::chrono::milliseconds(latencyMs_);
    return std::chrono::steady_clock::now() >= ready;
}

CameraHal::DeviceFrame SimCameraDevice::dequeueFrame(int timeoutMs) {
    DeviceFrame frame;
    frame.width = width_;
    frame.height = height_;
    if (!open_ || !streaming_) {
        return frame;
    }
    if (timeoutMs < 0) timeoutMs = 0;
    if (!capturing_) {
        captureStart_ = std::chrono::steady_clock::now();
        capturing_ = true;
    }
    // Poll in 5 ms steps until the frame is ready or the budget
    // runs out. This is the simulated equivalent of poll() on the
    // V4L2 descriptor with a timeout.
    std::chrono::steady_clock::time_point start =
        std::chrono::steady_clock::now();
    bool ready = false;
    while (std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - start)
               .count() < static_cast<double>(timeoutMs)) {
        if (frameReady()) {
            ready = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!ready) {
        capturing_ = false;
        return frame;  // timeout, valid == false
    }
    // Fill the next free ring buffer with the next frame.
    std::uint32_t idx = nextRing_;
    for (std::uint32_t i = 0; i < ring_.size(); ++i) {
        std::uint32_t cand = (nextRing_ + i) % static_cast<std::uint32_t>(ring_.size());
        if (!ringInUse_[cand]) {
            idx = cand;
            break;
        }
    }
    ringInUse_[idx] = true;
    nextRing_ = (idx + 1) % static_cast<std::uint32_t>(ring_.size());
    ++frameCounter_;
    generateRaw10Frame(ring_[idx].data(), width_, height_, frameCounter_);
    capturing_ = false;
    frame.data = ring_[idx].data();
    frame.size = static_cast<std::uint32_t>(raw10FrameBytes(width_, height_));
    frame.bufIndex = idx;
    frame.sequence = frameCounter_;
    frame.timestampUs =
        static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    frame.valid = true;
    return frame;
}

int SimCameraDevice::requeueBuffer(std::uint32_t bufIndex) {
    if (bufIndex >= ringInUse_.size()) return -EINVAL;
    ringInUse_[bufIndex] = false;
    return 0;
}

}  // namespace CameraHal
