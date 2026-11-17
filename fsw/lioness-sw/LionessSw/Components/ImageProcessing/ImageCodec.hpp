// ImageCodec.hpp
// Encoding helpers for the ImageProcessing component, matching
// sdd.md (Issue 44). Lossless compression is real zlib. The
// preview is encoded with stb_image_write, a single header
// library vendored in this directory (stb_image_write.h,
// public domain, https://github.com/nothings/stb): one library
// writes both the JPEG preview and the PNG preview, so the
// preview no longer depends on libjpeg headers being installed.
// The PNG preview is a viewable 320x240 8-bit grayscale image
// of the same downsampled frame as the JPEG. It is NOT a
// lossless product: PNG is 8-bit, so it cannot hold the RAW10
// frame byte for byte. The zlib lossless product is the only
// byte-for-byte archive. The source frame is never written to.
// A failed operation writes zero valid output bytes, as the
// design doc requires.
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef LionessSw_Components_ImageProcessing_ImageCodec_HPP
#define LionessSw_Components_ImageProcessing_ImageCodec_HPP

#include <cstddef>
#include <cstdint>

namespace ImageCodec {

// Preview size from the design doc, temporary development values.
const std::uint32_t kPreviewWidth = 320;
const std::uint32_t kPreviewHeight = 240;

// True when this build encodes real JPEG previews. Always true
// now that stb_image_write is vendored: no system library is
// needed for the preview.
bool previewIsJpeg();

// Downsample a RAW10 packed frame to a 320x240 preview and encode
// it as a grayscale JPEG at the given quality (1 to 100).
// Returns false and writes zero bytes on invalid input or when
// the destination is too small. scratch must hold at least
// 320 * 240 bytes and is used for the downsampled grayscale
// image before encoding.
bool makePreview(const std::uint8_t* source, std::size_t sourceBytes,
                 std::uint32_t width, std::uint32_t height,
                 std::uint32_t quality, std::uint8_t* dest,
                 std::size_t destCapacity, std::size_t& bytesWritten,
                 bool& isJpeg, std::uint8_t* scratch);

// Downsample a RAW10 packed frame to a 320x240 preview and encode
// it as a grayscale PNG. Same downsampled frame as makePreview.
// The PNG is 8-bit, so it is a viewable preview, not a lossless
// product. Returns false and writes zero bytes on invalid input
// or when the destination is too small. scratch must hold at
// least 320 * 240 bytes.
bool makePreviewPng(const std::uint8_t* source, std::size_t sourceBytes,
                    std::uint32_t width, std::uint32_t height,
                    std::uint8_t* dest, std::size_t destCapacity,
                    std::size_t& bytesWritten, std::uint8_t* scratch);

// Lossless compression with zlib. Returns false and writes zero
// bytes on invalid input or when the destination is too small.
// Use zlibBound() to size the destination.
std::size_t zlibBound(std::size_t sourceBytes);
bool compressLossless(const std::uint8_t* source, std::size_t sourceBytes,
                      std::uint8_t* dest, std::size_t destCapacity,
                      std::size_t& bytesWritten);

// Decompress a zlib stream produced by compressLossless, for
// tests and for ground verification. Returns false on any error.
bool decompressLossless(const std::uint8_t* source, std::size_t sourceBytes,
                        std::uint8_t* dest, std::size_t destCapacity,
                        std::size_t& bytesWritten);

}  // namespace ImageCodec

#endif
