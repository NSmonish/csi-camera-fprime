####
# F Prime CMakeLists.txt for the StarTrackerManager component.
# Style rule for this project: plain punctuation only, no em dashes.
####

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/StarTrackerManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/StarTrackerManager.cpp"
)

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/StarTrackerManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/StarTrackerManagerTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/StarTrackerManagerTester.cpp"
    DEPENDS
        STest
)
