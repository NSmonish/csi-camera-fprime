// videodev2_compat.h
// V4L2 API compatibility header for this project.
//
// On Linux it includes the real kernel header, <linux/videodev2.h>,
// and nothing changes. On macOS there is no V4L2 and no such header,
// so this file defines the small subset of the V4L2 API that the
// camera HAL actually uses: the structs, the pixel format constants,
// the capability flags, and the ioctl request numbers.
//
// Why this is safe: on macOS there is no V4L2 device, so every call
// into the real V4L2Camera fails at open time and returns an error,
// exactly as it would on Linux with no camera attached. The host
// deployment uses the simulated device, SimCameraDevice, which never
// touches a V4L2 ioctl. The only thing this file enables on macOS is
// compiling the HAL. It changes no behaviour on Linux.
//
// The ioctl numbers below are the real Linux values, copied from
// videodev2.h. They are only used so the code compiles on macOS.
// The struct layouts match the Linux kernel layouts for the fields
// this project touches.
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef VIDEODEV2_COMPAT_H
#define VIDEODEV2_COMPAT_H

#ifdef __linux__
#include <linux/videodev2.h>
#else

#include <cstdint>
#include <sys/time.h>

// Kernel integer types. The real header defines these on Linux.
typedef std::uint8_t __u8;
typedef std::uint16_t __u16;
typedef std::uint32_t __u32;
typedef std::int32_t __s32;
typedef std::uint64_t __u64;

// Four-character codes, as in the kernel header.
#define v4l2_fourcc(a, b, c, d) \
    ((__u32)(a) | ((__u32)(b) << 8) | ((__u32)(c) << 16) | ((__u32)(d) << 24))

// Pixel format used by this project: 10-bit Bayer, RGGB order.
#define V4L2_PIX_FMT_SRGGB10 v4l2_fourcc('R', 'G', '1', '0')

// Buffer types.
enum v4l2_buf_type {
    V4L2_BUF_TYPE_VIDEO_CAPTURE = 1,
    V4L2_BUF_TYPE_VIDEO_OUTPUT = 2,
    V4L2_BUF_TYPE_VIDEO_OVERLAY = 3,
};

// Memory types.
enum v4l2_memory {
    V4L2_MEMORY_MMAP = 1,
    V4L2_MEMORY_USERPTR = 2,
    V4L2_MEMORY_OVERLAY = 3,
    V4L2_MEMORY_DMABUF = 4,
};

// Field order.
enum v4l2_field {
    V4L2_FIELD_ANY = 0,
    V4L2_FIELD_NONE = 1,
    V4L2_FIELD_TOP = 2,
    V4L2_FIELD_BOTTOM = 3,
};

// Capability flags.
#define V4L2_CAP_VIDEO_CAPTURE 0x00000001
#define V4L2_CAP_VIDEO_CAPTURE_MPLANE 0x00001000
#define V4L2_CAP_DEVICE_CAPS 0x80000000

struct v4l2_capability {
    __u8 driver[16];
    __u8 card[32];
    __u8 bus_info[32];
    __u32 version;
    __u32 capabilities;
    __u32 device_caps;
    __u32 reserved[3];
};

struct v4l2_pix_format {
    __u32 width;
    __u32 height;
    __u32 pixelformat;
    __u32 field;
    __u32 bytesperline;
    __u32 sizeimage;
    __u32 colorspace;
    __u32 priv;
    __u32 flags;
    union {
        __u32 ycbcr_enc;
        __u32 hsv_enc;
    };
    __u32 quantization;
    __u32 xfer_func;
};

struct v4l2_rect {
    __s32 left;
    __s32 top;
    __u32 width;
    __u32 height;
};

struct v4l2_clip {
    struct v4l2_rect c;
    struct v4l2_clip* next;
};

struct v4l2_window {
    struct v4l2_rect w;
    __u32 field;
    __u32 chromakey;
    struct v4l2_clip* clips;
    __u32 clipcount;
    __u8* bitmap;
};

struct v4l2_format {
    __u32 type;
    union {
        struct v4l2_pix_format pix;
        struct v4l2_window win;
        __u8 raw_data[200];
    } fmt;
};
// Note: win is in the union because it carries the 8-byte aligned
// pointer that sets the union alignment. That is what makes
// sizeof(struct v4l2_format) 208 on 64-bit, matching the kernel
// header and the G_FMT and S_FMT ioctl numbers below.

struct v4l2_fmtdesc {
    __u32 index;
    __u32 type;
    __u32 flags;
    __u8 description[32];
    __u32 pixelformat;
    __u32 mbus_code;
    __u32 reserved[3];
};

struct v4l2_requestbuffers {
    __u32 count;
    __u32 type;
    __u32 memory;
    __u32 reserved[2];
};

struct v4l2_timeval {
    __u32 type;
    __u32 flags;
    __u8 frames;
    __u8 seconds;
    __u8 minutes;
    __u8 hours;
    __u8 userbits[4];
};

struct v4l2_buffer {
    __u32 index;
    __u32 type;
    __u32 bytesused;
    __u32 flags;
    __u32 field;
    struct timeval timestamp;
    struct v4l2_timeval timecode;
    __u32 sequence;
    __u32 memory;
    union {
        __u32 offset;
        unsigned long userptr;
        struct v4l2_plane* planes;
        __s32 fd;
    } m;
    __u32 length;
    __u32 reserved[2];
    __u32 request_fd;
};

struct v4l2_control {
    __u32 id;
    __s32 value;
};

struct v4l2_queryctrl {
    __u32 id;
    __u32 type;
    __u8 name[32];
    __s32 minimum;
    __s32 maximum;
    __s32 step;
    __s32 default_value;
    __u32 flags;
    __u32 reserved[2];
};

// Linux ioctl request numbers, from videodev2.h. On macOS no V4L2
// device exists, so every ioctl using them fails, and the HAL
// reports that failure. The numbers only exist so the code compiles.
#define VIDIOC_QUERYCAP 0x80685600
#define VIDIOC_ENUM_FMT 0xC0405602
#define VIDIOC_G_FMT 0xC0D05604
#define VIDIOC_S_FMT 0xC0D05605
#define VIDIOC_REQBUFS 0xC0145608
#define VIDIOC_QUERYBUF 0xC0585609
#define VIDIOC_QBUF 0xC058560F
#define VIDIOC_DQBUF 0xC0585611
#define VIDIOC_STREAMON 0x40045612
#define VIDIOC_STREAMOFF 0x40045613
#define VIDIOC_G_CTRL 0xC008561B
#define VIDIOC_S_CTRL 0xC008561C
#define VIDIOC_QUERYCTRL 0xC0445624

#endif  // __linux__

#endif  // VIDEODEV2_COMPAT_H
