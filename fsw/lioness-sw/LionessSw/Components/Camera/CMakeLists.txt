####
# F Prime CMakeLists.txt for the Camera component.
# Style rule for this project: plain punctuation only, no em dashes.
####

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Camera.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Camera.cpp"
    DEPENDS
        LionessSw_V4L2
)

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Camera.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/CameraTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/CameraTester.cpp"
    DEPENDS
        STest
        LionessSw_V4L2
)
