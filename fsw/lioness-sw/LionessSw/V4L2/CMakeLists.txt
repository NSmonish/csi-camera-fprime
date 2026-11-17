####
# F Prime CMakeLists.txt for the V4L2 camera HAL library.
#
# Plain C++ library, no FPP inputs, no F Prime dependencies.
# V4L2Camera is the real driver wrapper from PR 55, with the
# blocking dequeueFrame() fixed to poll with a timeout.
# SimCameraDevice is the simulated device for host testing.
# The Camera component depends on this library.
# Style rule for this project: plain punctuation only, no em dashes.
####

register_fprime_library(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/V4L2CameraBase.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/V4L2Camera.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/SimCameraDevice.cpp"
)
