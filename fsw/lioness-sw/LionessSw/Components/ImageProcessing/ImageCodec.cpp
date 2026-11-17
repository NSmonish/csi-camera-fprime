// ImageCodec.cpp
// See ImageCodec.hpp. Lossless is real zlib. Preview is encoded
// with stb_image_write (vendored stb_image_write.h, public
// domain, https://github.com/nothings/stb): one library writes
// both the JPEG preview and the PNG preview, with no system
// library dependency. The PNG is a viewable 8-bit preview, not
// a lossless product.
// Style rule for this project: plain punctuation only, no em dashes.

#include "LionessSw/Components/ImageProcessing/ImageCodec.hpp"

#include <cstdlib>
#include <cstring>
#include <zlib.h>

#include "LionessSw/V4L2/SimCameraDevice.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "LionessSw/Components/ImageProcessing/stb_image_write.h"

namespace ImageCodec {

bool previewIsJpeg() {
    return true;
}

namespace {

// Downsample a RAW10 frame to 320x240 8-bit grayscale by nearest
// neighbour sampling, 10-bit to 8-bit with a right shift of 2.
bool downsample(const std::uint8_t* source, std::size_t sourceBytes,
                std::uint32_t width, std::uint32_t height,
                std::uint8_t* dest) {
    if (source == nullptr || dest == nullptr) return false;
    if (width == 0 || height == 0) return false;
    if (width < kPreviewWidth || height < kPreviewHeight) return false;
    if (!CameraHal::isSupportedMode(width, height)) return false;
    if (sourceBytes < CameraHal::raw10FrameBytes(width, height)) return false;
    for (std::uint32_t py = 0; py < kPreviewHeight; ++py) {
        std::uint32_t sy = py * height / kPreviewHeight;
        for (std::uint32_t px = 0; px < kPreviewWidth; ++px) {
            std::uint32_t sx = px * width / kPreviewWidth;
            std::size_t idx =
                static_cast<std::size_t>(sy) * static_cast<std::size_t>(width) + sx;
            std::uint16_t value10 =
                CameraHal::unpackRaw10Pixel(source, idx);
            dest[static_cast<std::size_t>(py) * kPreviewWidth + px] =
                static_cast<std::uint8_t>(value10 >> 2);
        }
    }
    return true;
}

// Growable sink for the stb JPEG writer. It appends encoded
// bytes to a fixed caller buffer and counts the total, so an
// encode that does not fit is detected after the call.
struct MemWriter {
    std::uint8_t* dest;
    std::size_t capacity;
    std::size_t used;
};

void jpgWriteFunc(void* context, void* data, int size) {
    MemWriter* writer = static_cast<MemWriter*>(context);
    if (size <= 0) return;
    if (writer->used + static_cast<std::size_t>(size) <= writer->capacity) {
        std::memcpy(writer->dest + writer->used, data,
                    static_cast<std::size_t>(size));
    }
    writer->used += static_cast<std::size_t>(size);
}

}  // namespace

bool makePreview(const std::uint8_t* source, std::size_t sourceBytes,
                 std::uint32_t width, std::uint32_t height,
                 std::uint32_t quality, std::uint8_t* dest,
                 std::size_t destCapacity, std::size_t& bytesWritten,
                 bool& isJpeg, std::uint8_t* scratch) {
    bytesWritten = 0;
    isJpeg = false;
    if (source == nullptr || dest == nullptr || scratch == nullptr) {
        return false;
    }
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;
    if (!downsample(source, sourceBytes, width, height, scratch)) {
        return false;
    }
    // Encode the downsampled grayscale image as a JPEG with stb.
    // stb has no jpg-to-memory call, so the JPEG writer calls
    // back into jpgWriteFunc, which appends to the caller
    // buffer. The writer stops being useful past the capacity:
    // jpgWriteFunc keeps counting bytes, and the call fails
    // when the total does not fit.
    MemWriter writer;
    writer.dest = dest;
    writer.capacity = destCapacity;
    writer.used = 0;
    int wrote = stbi_write_jpg_to_func(
        jpgWriteFunc, &writer, static_cast<int>(kPreviewWidth),
        static_cast<int>(kPreviewHeight), 1, scratch,
        static_cast<int>(quality));
    if (wrote == 0 || writer.used == 0 ||
        writer.used > destCapacity) {
        return false;
    }
    bytesWritten = writer.used;
    isJpeg = true;
    return true;
}

bool makePreviewPng(const std::uint8_t* source, std::size_t sourceBytes,
                    std::uint32_t width, std::uint32_t height,
                    std::uint8_t* dest, std::size_t destCapacity,
                    std::size_t& bytesWritten, std::uint8_t* scratch) {
    bytesWritten = 0;
    if (source == nullptr || dest == nullptr || scratch == nullptr) {
        return false;
    }
    if (!downsample(source, sourceBytes, width, height, scratch)) {
        return false;
    }
    // Encode the same downsampled grayscale image as a PNG.
    // The PNG is 8-bit, so it is a viewable preview, not a
    // lossless product: it cannot hold the RAW10 frame.
    int outLen = 0;
    unsigned char* encoded = stbi_write_png_to_mem(
        scratch, static_cast<int>(kPreviewWidth),
        static_cast<int>(kPreviewWidth), static_cast<int>(kPreviewHeight),
        1, &outLen);
    if (encoded == nullptr || outLen <= 0) {
        free(encoded);
        return false;
    }
    bool ok = false;
    if (static_cast<std::size_t>(outLen) <= destCapacity) {
        std::memcpy(dest, encoded, static_cast<std::size_t>(outLen));
        bytesWritten = static_cast<std::size_t>(outLen);
        ok = true;
    }
    free(encoded);
    return ok;
}

std::size_t zlibBound(std::size_t sourceBytes) {
    return compressBound(static_cast<uLong>(sourceBytes));
}

bool compressLossless(const std::uint8_t* source, std::size_t sourceBytes,
                      std::uint8_t* dest, std::size_t destCapacity,
                      std::size_t& bytesWritten) {
    bytesWritten = 0;
    if (source == nullptr || dest == nullptr) return false;
    if (sourceBytes == 0) return false;
    if (destCapacity < zlibBound(sourceBytes)) return false;
    uLongf outLen = static_cast<uLongf>(destCapacity);
    int rc = compress2(dest, &outLen, source, static_cast<uLong>(sourceBytes),
                       Z_DEFAULT_COMPRESSION);
    if (rc != Z_OK) return false;
    bytesWritten = outLen;
    return true;
}

bool decompressLossless(const std::uint8_t* source, std::size_t sourceBytes,
                        std::uint8_t* dest, std::size_t destCapacity,
                        std::size_t& bytesWritten) {
    bytesWritten = 0;
    if (source == nullptr || dest == nullptr) return false;
    if (sourceBytes == 0 || destCapacity == 0) return false;
    uLongf outLen = static_cast<uLongf>(destCapacity);
    int rc = uncompress(dest, &outLen, source, static_cast<uLong>(sourceBytes));
    if (rc != Z_OK) return false;
    bytesWritten = outLen;
    return true;
}

}  // namespace ImageCodec
