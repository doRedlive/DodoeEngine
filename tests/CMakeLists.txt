set(tests_folder "Tests")

option(DODOE_BUILD_UNIT_TESTS "Build unit tests" ON)
option(DODOE_BUILD_INTEGRATION_TESTS "Build integration tests" ON)
option(DODOE_BUILD_SMOKE_TESTS "Register smoke tests" OFF)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(unit/googletest EXCLUDE_FROM_ALL)
set_target_properties(gtest gtest_main PROPERTIES FOLDER "${tests_folder}/googletest")

enable_testing()

set(DODOE_TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")

function(dodoe_add_test TEST_NAME)
    cmake_parse_arguments(ARG "" "LABEL;TIMEOUT" "SOURCES" ${ARGN})
    add_executable(${TEST_NAME} ${ARG_SOURCES})
    target_link_libraries(${TEST_NAME} PRIVATE gtest_main DodoeRuntime)
    target_include_directories(${TEST_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/engine/src/runtime")
    target_compile_definitions(${TEST_NAME} PRIVATE DODOE_TEST_DATA_DIR="${DODOE_TEST_DATA_DIR}")
    set_target_properties(${TEST_NAME} PROPERTIES
        FOLDER "${tests_folder}"
        RUNTIME_OUTPUT_DIRECTORY "${BINARY_ROOT_DIR}"
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES LABELS "${ARG_LABEL}")
    if(ARG_TIMEOUT)
        set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT "${ARG_TIMEOUT}")
    endif()
endfunction()

if(DODOE_BUILD_UNIT_TESTS)
    add_subdirectory(unit)
endif()
if(DODOE_BUILD_INTEGRATION_TESTS)
    add_subdirectory(integration)
endif()
if(DODOE_BUILD_SMOKE_TESTS)
    add_subdirectory(smoke)
endif()
