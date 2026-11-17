####
# F Prime CMakeLists.txt for the PayloadManager component.
# Style rule for this project: plain punctuation only, no em dashes.
####

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/PayloadManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/PayloadManager.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/StorageStub.cpp"
)

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/PayloadManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/PayloadManagerTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/PayloadManagerTester.cpp"
    DEPENDS
        STest
)
