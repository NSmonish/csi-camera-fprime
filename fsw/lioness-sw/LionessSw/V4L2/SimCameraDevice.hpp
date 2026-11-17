// SimCameraDevice.hpp
// Simulated camera device implementing CameraHal::ICameraDevice.
// It stands in for a real V4L2 device so the whole camera stack
// can be built and tested on the host with no hardware. Frames are
// synthetic RAW10 packed frames from the verified simulation in
// workspace/csi-camera-capture: a Bayer-like gradient, value =
// (3*x + 5*y + 11*counter) mod 1024, with the frame counter in the
// first four pixels of row zero. The payload camera models the
// ArduCam 12MP Module 3 (Sony IMX708, 4608x2592, RAW10, MIPI CSI-2).
// The star tracker camera models a 1280x720 RAW10 frame on Parallel
// CSI. That star camera format is an assumption, not a measured
// fact: the team has never given the star sensor or its format, so
// confirm it before flight. No F Prime dependencies.
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef LionessSw_V4L2_SimCameraDevice_HPP
#define LionessSw_V4L2_SimCameraDevice_HPP

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "CameraDevice.hpp"

namespace CameraHal {

// RAW10 packing: four 10-bit pixels p0..p3 pack into five bytes,
// most significant bits first:
//   b0 = p0 >> 2
//   b1 = ((p0 & 3) << 6) | (p1 >> 4)
//   b2 = ((p1 & 15) << 4) | (p2 >> 6)
//   b3 = ((p2 & 63) << 2) | (p3 >> 8)
//   b4 = p3 & 0xFF
// Unpacking inverts this exactly.
std::uint16_t unpackRaw10Pixel(const std::uint8_t* packed, std::size_t pixelIndex);

// Bytes for one RAW10 packed frame. No row padding in this
// simulation, so stride is width * 10 / 8 bytes.
inline std::size_t raw10FrameBytes(std::uint32_t width, std::uint32_t height) {
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 10 / 8;
}

// Supported modes. Payload camera: full IMX708 list. Star camera:
// 1280x720 is the assumed mode, 640x480 is kept for tests.
inline bool isSupportedMode(std::uint32_t width, std::uint32_t height) {
    return (width == 640 && height == 480) ||
           (width == 1280 && height == 720) ||
           (width == 2304 && height == 1296) ||
           (width == 4608 && height == 2592);
}

// Fill dest with a synthetic RAW10 frame for the given mode and
// frame counter. dest must hold raw10FrameBytes(width, height).
void generateRaw10Frame(std::uint8_t* dest, std::uint32_t width,
                        std::uint32_t height, std::uint32_t frameCounter);

// Read back the frame counter from a generated frame.
std::uint32_t readFrameCounter(const std::uint8_t* frame, std::uint32_t width,
                               std::uint32_t height);

// FNV-1a 64-bit checksum, used to verify byte-for-byte equality.
inline std::uint64_t fnv1a(const std::uint8_t* data, std::size_t len) {
    std::uint64_t h = 14695981039346656037ULL;
    for (std::size_t i = 0; i < len; ++i) {
        h ^= static_cast<std::uint64_t>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

class SimCameraDevice : public ICameraDevice {
public:
    // label is only for logging, e.g. "payload" or "star". The
    // default mode is 1280x720; setFormat() changes it.
    explicit SimCameraDevice(const std::string& label);

    // ICameraDevice
    int open(const std::string& devicePath) override;
    bool isOpen() const override { return open_; }
    void close() override;
    int setFormat(std::uint32_t width, std::uint32_t height,
                  std::uint32_t pixelFormat) override;
    void getFormat(std::uint32_t& width, std::uint32_t& height,
                   std::uint32_t& pixelFormat) const override;
    int allocateBuffers(std::uint32_t count) override;
    int startStreaming() override;
    int stopStreaming() override;
    bool isStreaming() const override { return streaming_; }
    DeviceFrame dequeueFrame(int timeoutMs) override;
    int requeueBuffer(std::uint32_t bufIndex) override;
    int getFd() const override { return -1; }
    bool isSimulated() const override { return true; }

    // Test hooks. Latency is the simulated sensor delay before a
    // frame is ready, default 120 ms. failTimeout makes frames
    // never arrive, so dequeueFrame() times out.
    void setLatencyMs(std::uint32_t ms) { latencyMs_ = ms; }
    void setFailTimeout(bool fail) { failTimeout_ = fail; }
    std::uint32_t frameCounter() const { return frameCounter_; }
    const std::string& label() const { return label_; }

private:
    bool frameReady() const;

    std::string label_;
    bool open_;
    bool streaming_;
    std::uint32_t width_;
    std::uint32_t height_;
    std::uint32_t pixelFormat_;
    std::uint32_t latencyMs_;
    bool failTimeout_;
    std::uint32_t frameCounter_;
    bool capturing_;
    std::chrono::steady_clock::time_point captureStart_;
    // Preallocated ring buffers, like the V4L2 MMAP ring.
    std::vector<std::vector<std::uint8_t> > ring_;
    std::vector<bool> ringInUse_;
    std::uint32_t nextRing_;
};

}  // namespace CameraHal

#endif
