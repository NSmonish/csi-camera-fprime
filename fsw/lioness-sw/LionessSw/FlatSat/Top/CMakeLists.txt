####
# F Prime CMakeLists.txt:
#
# SOURCES: list of source files (to be compiled)
# AUTOCODER_INPUTS: list of files to be passed to the autocoders
# DEPENDS: list of libraries that this module depends on
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/reference/api/cmake/API/
#
####

register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/instances.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/topology.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/FlatSatTopology.cpp"
    DEPENDS
        Fw_Logger
)

### Unit Tests ###
# End-to-end host test for the camera stack. The real
# components are wired exactly as topology.fpp wires them,
# with the simulated devices behind the Camera component.
# There is no FPP of its own for this test.
register_fprime_ut(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/CameraStackTestMain.cpp"
    DEPENDS
        STest
        LionessSw_V4L2
        LionessSw_Components_Camera
        LionessSw_Components_ImageProcessing
        LionessSw_Components_PayloadManager
        LionessSw_Components_StarTrackerManager
        LionessSw_Components_StarTracker
)
