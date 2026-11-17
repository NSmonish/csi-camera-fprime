// verify_real_frame.cpp
// Check a real RAW10 frame file from the ArduCam IMX708 against the
// assumptions the F Prime camera stack was built on, and write viewable
// previews. This is the Wednesday Nov 18, 2026 hardware test tool.
// It runs on a laptop, not on the flight computer, and it reads only
// the frame file you give it.
//
// Usage:
//   verify_real_frame <frame.raw> <width> <height> [strideBytes] [outPrefix]
//   frame.raw    raw bytes from the camera (rpicam-still --raw, or V4L2)
//   width        frame width in pixels, for example 4608
//   height       frame height in pixels, for example 2592
//   strideBytes  optional row stride in bytes. Default is width * 10 / 8,
//                the packed stride with no row padding. If the file is
//                bigger than width * height * 10 / 8, pass the stride
//                reported by v4l2-ctl or libcamera and the tool checks it.
//   outPrefix    optional output prefix, default "checked". Writes
//                <prefix>_preview.png (320x240) and <prefix>_full8.png
//                (full frame, 8 bit grayscale) via stb_image_write.
//
// What it checks, and why each check matters:
//   1. File size matches width * height * 10 / 8, or stride * height
//      when a stride is given. A mismatch means the driver pads rows
//      or the format is not RAW10 packed, and the stack's stride
//      assumption (width * 10 / 8) is wrong for this camera.
//   2. Pixel values unpack cleanly in the documented RAW10 layout:
//      four 10 bit pixels in five bytes, most significant bits first.
//      A wrong layout shows up as garbage values, so the tool reports
//      min, max, mean and the share of saturated (1023) pixels.
//   3. No black or stuck rows: it reports the mean of the first, middle
//      and last rows. A dead row or a torn frame shows up here.
//   4. The preview PNGs it writes let you look at the frame with your
//      eyes, which is the fastest way to spot a wrong Bayer order,
//      a flipped image, or a bad exposure.
//
// Exit code 0 means every check passed. Exit code 1 means at least one
// check failed, and the tool prints which one. A failed check does not
// mean the camera is broken, it means the stack's assumption about the
// frame does not match this camera, and the assumption must be fixed
// before the F Prime stack runs on it.
// Style rule for this project: plain punctuation only, no em dashes.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Unpack one 10 bit pixel from a RAW10 packed buffer. Four pixels
// p0..p3 pack into five bytes, most significant bits first:
//   b0 = p0 >> 2
//   b1 = ((p0 & 3) << 6) | (p1 >> 4)
//   b2 = ((p1 & 15) << 4) | (p2 >> 6)
//   b3 = ((p2 & 63) << 2) | (p3 >> 8)
//   b4 = p3 & 0xFF
static std::uint16_t unpackRaw10Pixel(const std::uint8_t* packed,
                                      std::size_t pixelIndex) {
    std::size_t group = pixelIndex / 4;
    std::size_t slot = pixelIndex % 4;
    const std::uint8_t* b = packed + group * 5;
    switch (slot) {
        case 0:
            return static_cast<std::uint16_t>((b[0] << 2) | (b[1] >> 6));
        case 1:
            return static_cast<std::uint16_t>(((b[1] & 0x3F) << 4) |
                                              (b[2] >> 4));
        case 2:
            return static_cast<std::uint16_t>(((b[2] & 0x0F) << 6) |
                                              (b[3] >> 2));
        default:
            return static_cast<std::uint16_t>(((b[3] & 0x03) << 8) | b[4]);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::fprintf(stderr,
                     "Usage: %s <frame.raw> <width> <height> "
                     "[strideBytes] [outPrefix]\n",
                     argv[0]);
        return 2;
    }
    const char* path = argv[1];
    std::uint32_t width = static_cast<std::uint32_t>(std::atoi(argv[2]));
    std::uint32_t height = static_cast<std::uint32_t>(std::atoi(argv[3]));
    std::size_t packedStride = static_cast<std::size_t>(width) * 10 / 8;
    std::size_t stride = packedStride;
    std::string outPrefix = "checked";
    // The 4th argument is the stride in bytes when it is a number, or
    // the output prefix when it is not. This keeps the common call
    // "verify_real_frame frame.raw 1280 720 checked720" working.
    if (argc >= 5) {
        char* end = nullptr;
        long value = std::strtol(argv[4], &end, 10);
        if (end != nullptr && *end == '\0' && value > 0) {
            stride = static_cast<std::size_t>(value);
        } else {
            outPrefix = argv[4];
        }
    }
    if (argc >= 6) outPrefix = argv[5];
    if (stride < packedStride) {
        std::fprintf(stderr,
                     "Bad stride %zu: it cannot be smaller than the packed "
                     "stride %zu\n",
                     stride, packedStride);
        return 2;
    }

    if (width == 0 || height == 0 || width % 4 != 0) {
        std::fprintf(stderr, "Bad dimensions: width must be a multiple of 4\n");
        return 2;
    }

    FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        std::fprintf(stderr, "Cannot open %s\n", path);
        return 2;
    }
    std::fseek(f, 0, SEEK_END);
    long fileSize = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(fileSize));
    if (std::fread(data.data(), 1, data.size(), f) != data.size()) {
        std::fprintf(stderr, "Short read on %s\n", path);
        std::fclose(f);
        return 2;
    }
    std::fclose(f);

    bool ok = true;
    std::size_t expectedPacked =
        static_cast<std::size_t>(width) * height * 10 / 8;
    std::size_t expectedStrided = stride * height;
    std::printf("Frame: %s\n", path);
    std::printf("Dimensions: %ux%u, file size %ld bytes\n", width, height,
                fileSize);
    std::printf("Packed size (no padding): %zu bytes\n", expectedPacked);
    std::printf("Stride: %zu bytes per row\n", stride);

    if (static_cast<std::size_t>(fileSize) == expectedPacked &&
        stride == packedStride) {
        std::printf("[PASS] Size matches packed RAW10, no row padding\n");
    } else if (static_cast<std::size_t>(fileSize) == expectedStrided &&
               stride > packedStride) {
        std::printf("[PASS] Size matches stride %zu, rows are padded by "
                    "%zu bytes\n",
                    stride, stride - packedStride);
        std::printf("       The F Prime stack assumes stride %zu, so it "
                    "must be told about this padding before it runs on "
                    "this camera.\n",
                    packedStride);
    } else {
        std::printf("[FAIL] Size matches neither packed (%zu) nor strided "
                    "(%zu). The format is not RAW10 packed at this stride, "
                    "or the dimensions are wrong.\n",
                    expectedPacked, expectedStrided);
        ok = false;
    }

    // Never unpack a file that is shorter than the frame it claims to
    // be. Reading past the end of the buffer is a crash, not a check.
    std::size_t needed =
        (stride == packedStride) ? expectedPacked : expectedStrided;
    if (static_cast<std::size_t>(fileSize) < needed) {
        std::printf("File is %ld bytes short of the %zu bytes this frame "
                    "needs. Stopping here.\n",
                    static_cast<long>(needed - fileSize), needed);
        std::printf("Result: CHECKS FAILED\n");
        return 1;
    }

    // Unpack every pixel, or a sample of them at full resolution so the
    // check stays fast. Full resolution is 11.9 MP, sampling every 16th
    // pixel still covers the whole frame.
    std::uint16_t minVal = 1023;
    std::uint16_t maxVal = 0;
    std::uint64_t sum = 0;
    std::uint64_t saturated = 0;
    std::uint64_t checked = 0;
    std::size_t total = static_cast<std::size_t>(width) * height;
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::uint8_t* row = data.data() + y * stride;
        for (std::uint32_t x = 0; x < width; x += 16) {
            std::uint16_t v = unpackRaw10Pixel(row, x);
            if (v < minVal) minVal = v;
            if (v > maxVal) maxVal = v;
            sum += v;
            if (v == 1023) ++saturated;
            ++checked;
        }
    }
    double mean = static_cast<double>(sum) / static_cast<double>(checked);
    std::printf("Pixels sampled: %llu of %zu\n",
                static_cast<unsigned long long>(checked), total);
    std::printf("Min %u, max %u, mean %.1f, saturated %.2f%%\n", minVal, maxVal,
                mean, 100.0 * static_cast<double>(saturated) /
                          static_cast<double>(checked));
    if (maxVal == 0) {
        std::printf("[FAIL] Frame is all black. Check exposure, lens cap "
                    "and camera connection.\n");
        ok = false;
    } else if (minVal == 1023) {
        std::printf("[FAIL] Frame is all saturated. Check exposure.\n");
        ok = false;
    } else {
        std::printf("[PASS] Pixel values look like a real image\n");
    }
    if (static_cast<double>(saturated) / static_cast<double>(checked) > 0.5) {
        std::printf("[WARN] More than half the sampled pixels are "
                    "saturated. The frame may be overexposed.\n");
    }

    // Row means for the first, middle and last rows, to catch a dead
    // row or a torn frame.
    std::uint32_t rows[3] = {0, height / 2, height - 1};
    double rowMeans[3] = {0.0, 0.0, 0.0};
    for (int r = 0; r < 3; ++r) {
        const std::uint8_t* row = data.data() + rows[r] * stride;
        std::uint64_t rowSum = 0;
        std::uint32_t rowCount = 0;
        for (std::uint32_t x = 0; x < width; x += 16) {
            rowSum += unpackRaw10Pixel(row, x);
            ++rowCount;
        }
        rowMeans[r] = static_cast<double>(rowSum) / rowCount;
        std::printf("Row %u mean: %.1f\n", rows[r], rowMeans[r]);
    }
    if (rowMeans[0] == 0.0 || rowMeans[1] == 0.0 || rowMeans[2] == 0.0) {
        std::printf("[FAIL] A checked row is all black.\n");
        ok = false;
    } else {
        std::printf("[PASS] First, middle and last rows are non-black\n");
    }

    // Write a full frame 8 bit preview and a 320x240 preview, both PNG.
    std::vector<std::uint8_t> full8(total);
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::uint8_t* row = data.data() + y * stride;
        for (std::uint32_t x = 0; x < width; ++x) {
            full8[static_cast<std::size_t>(y) * width + x] =
                static_cast<std::uint8_t>(unpackRaw10Pixel(row, x) >> 2);
        }
    }
    std::string fullPath = outPrefix + "_full8.png";
    if (!stbi_write_png(fullPath.c_str(), static_cast<int>(width),
                        static_cast<int>(height), 1, full8.data(),
                        static_cast<int>(width))) {
        std::fprintf(stderr, "Failed to write %s\n", fullPath.c_str());
        return 1;
    }
    const int pw = 320;
    const int ph = 240;
    std::vector<std::uint8_t> preview(static_cast<std::size_t>(pw) * ph);
    for (int py = 0; py < ph; ++py) {
        std::uint32_t sy = static_cast<std::uint32_t>(py) * height / ph;
        for (int px = 0; px < pw; ++px) {
            std::uint32_t sx = static_cast<std::uint32_t>(px) * width / pw;
            preview[static_cast<std::size_t>(py) * pw + px] =
                full8[static_cast<std::size_t>(sy) * width + sx];
        }
    }
    std::string previewPath = outPrefix + "_preview.png";
    if (!stbi_write_png(previewPath.c_str(), pw, ph, 1, preview.data(), pw)) {
        std::fprintf(stderr, "Failed to write %s\n", previewPath.c_str());
        return 1;
    }
    std::printf("Wrote %s and %s\n", fullPath.c_str(), previewPath.c_str());
    std::printf("Result: %s\n", ok ? "ALL CHECKS PASSED" : "CHECKS FAILED");
    return ok ? 0 : 1;
}
