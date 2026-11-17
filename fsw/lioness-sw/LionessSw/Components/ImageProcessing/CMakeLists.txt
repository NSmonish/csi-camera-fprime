####
# F Prime CMakeLists.txt for ImageProcessing (Issue 44).
# Style rule for this project: plain punctuation only, no em dashes.
####

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/ImageProcessing.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ImageProcessing.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ImageCodec.cpp"
    DEPENDS
        LionessSw_V4L2
)

# The lossless codec is real zlib. Link it here so the
# deployment and the unit test both get it.
find_package(ZLIB REQUIRED)
target_link_libraries(LionessSw_Components_ImageProcessing PUBLIC ZLIB::ZLIB)

# The preview codec is stb_image_write, vendored as a single
# header in this directory (stb_image_write.h, public domain).
# It writes both the JPEG preview and the PNG preview, so no
# system image library is needed and no JPEG package is linked.

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/ImageProcessing.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ImageProcessingTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ImageProcessingTester.cpp"
    DEPENDS
        STest
        LionessSw_V4L2
)
