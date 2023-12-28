include_directories(${libvidio_BINARY_DIR} ${libvidio_SOURCE_DIR} ${libvidio_SOURCE_DIR}/includes)

# prepare C++ configuration file (test-config.cc)

set(TESTING_DATA_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data")
# configure_file(test-config.cc.in ${CMAKE_BINARY_DIR}/generated/test-config.cc)

macro(add_libvidio_test TEST_FILE)
    set(TEST_NAME ${TEST_FILE})
    add_executable(${TEST_NAME} main.cc catch.hpp ${CMAKE_BINARY_DIR}/generated/test-config.cc test_utils.cc ${TEST_FILE}.cc)
    target_link_libraries(${TEST_NAME} PRIVATE vidio)
    add_test(NAME ${TEST_NAME} COMMAND ./${TEST_NAME})
endmacro()

# --- tests that require access to internal symbols

if (WITH_REDUCED_VISIBILITY)
    message(WARNING "... tests can only be compiled with full symbol visibility (WITH_REDUCED_VISIBILITY=OFF)")
else()
    add_libvidio_test(conversion)
endif()

# --- tests that only access the public API

# add_libvidio_test(encode)
