// CameraDevice.hpp
// Abstract camera device interface shared by the real V4L2 driver
// wrapper and the simulated camera. The Camera F Prime component
// talks only to this interface, so the same component code runs
// against simulated frames on the host and against real hardware
// on the flight computer. The only difference is which
// implementation is injected in FlatSatTopology.cpp.
// No F Prime dependencies.
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef LionessSw_V4L2_CameraDevice_HPP
#define LionessSw_V4L2_CameraDevice_HPP

#include <cstdint>
#include <string>

namespace CameraHal {

// One dequeued frame. Non-owning: data points into a buffer owned
// by the device ring. The caller must call requeueBuffer() when it
// is done with the frame, before the next dequeue, or the driver
// starves of buffers.
struct DeviceFrame {
    const std::uint8_t* data;  // frame bytes, nullptr when invalid
    std::uint32_t size;        // bytes actually filled
    std::uint32_t bufIndex;    // ring index, for requeueBuffer()
    std::uint32_t sequence;    // V4L2 buffer sequence, or sim frame counter
    std::uint64_t timestampUs; // capture timestamp, microseconds
    std::uint32_t width;
    std::uint32_t height;
    bool valid;

    DeviceFrame()
        : data(nullptr), size(0), bufIndex(0), sequence(0), timestampUs(0),
          width(0), height(0), valid(false) {}
};

// Abstract camera device. All methods return 0 on success or a
// negative errno-style code on failure, matching the PR 55 V4L2
// convention. dequeueFrame() is the fixed version of the PR 55
// call: it waits at most timeoutMs for a frame and never blocks
// forever. A return with valid == false means timeout or error,
// not a frame.
class ICameraDevice {
public:
    virtual ~ICameraDevice() {}

    // Open the device. For the real driver this is a /dev/videoN
    // path, or empty to auto-detect the first capture device.
    // For the simulated device the path is a label only.
    virtual int open(const std::string& devicePath) = 0;

    // True when the device is open.
    virtual bool isOpen() const = 0;

    // Close the device and release all resources.
    virtual void close() = 0;

    // Propose a pixel format and resolution. The driver may adjust
    // the values; call getFormat() after this to read back what
    // was actually set.
    virtual int setFormat(std::uint32_t width, std::uint32_t height,
                          std::uint32_t pixelFormat) = 0;
    virtual void getFormat(std::uint32_t& width, std::uint32_t& height,
                           std::uint32_t& pixelFormat) const = 0;

    // Allocate count ring buffers. Must be called before streaming.
    virtual int allocateBuffers(std::uint32_t count) = 0;

    // Start and stop the capture stream.
    virtual int startStreaming() = 0;
    virtual int stopStreaming() = 0;
    virtual bool isStreaming() const = 0;

    // Wait up to timeoutMs for the next frame, then dequeue it.
    // Returns a frame with valid == true on success, and a frame
    // with valid == false on timeout or error. Never blocks longer
    // than timeoutMs plus one poll step.
    virtual DeviceFrame dequeueFrame(int timeoutMs) = 0;

    // Hand a dequeued buffer back to the device.
    virtual int requeueBuffer(std::uint32_t bufIndex) = 0;

    // File descriptor for poll(), or -1 when the device has none
    // (simulated device). The real V4L2 implementation uses poll()
    // on this descriptor inside dequeueFrame().
    virtual int getFd() const = 0;

    // True for the simulated device, false for the real driver.
    virtual bool isSimulated() const = 0;
};

}  // namespace CameraHal

#endif
