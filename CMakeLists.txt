cmake_minimum_required(VERSION 3.20)
project(Dodoe VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(
        FATAL_ERROR
        "In-source builds not allowed. Please make a new directory (called a 'build' directory) and run CMake from there."
    )
endif()

set(DODOE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${DODOE_ROOT_DIR}/bin" CACHE PATH "" FORCE)
endif()
set(BINARY_ROOT_DIR "${CMAKE_INSTALL_PREFIX}")

add_compile_definitions(DODOE_ROOT="${DODOE_ROOT_DIR}")
add_compile_definitions("ImDrawIdx=unsigned int")

option(DODOE_SHIPPING "Shipping build" OFF)
add_compile_definitions($<$<BOOL:${DODOE_SHIPPING}>:DODOE_SHIPPING=1>)

if(MSVC)
    add_compile_definitions(_ENABLE_EXTENDED_ALIGNED_STORAGE)
endif()

if(WIN32 AND NOT DEFINED CMAKE_PREFIX_PATH AND NOT DEFINED Qt6_DIR)
    file(GLOB DODOE_QT_CANDIDATES
        "C:/Qt/[0-9]*/*/lib/cmake/Qt6"
        "D:/Qt/[0-9]*/*/lib/cmake/Qt6"
        "E:/Qt/[0-9]*/*/lib/cmake/Qt6"
        "$ENV{USERPROFILE}/Library/Qt/[0-9]*/*/lib/cmake/Qt6"
    )
    if(DODOE_QT_CANDIDATES)
        list(SORT DODOE_QT_CANDIDATES)
        list(GET DODOE_QT_CANDIDATES -1 DODOE_QT_LATEST)
        get_filename_component(DODOE_QT_PREFIX "${DODOE_QT_LATEST}/../../.." ABSOLUTE)
        list(APPEND CMAKE_PREFIX_PATH "${DODOE_QT_PREFIX}")
        message(STATUS "Dodoe: auto-detected Qt at ${DODOE_QT_PREFIX}")
    endif()
    unset(DODOE_QT_CANDIDATES)
    unset(DODOE_QT_LATEST)
    unset(DODOE_QT_PREFIX)
endif()

add_subdirectory(engine)

option(DODOE_BUILD_TESTS "Build Dodoe tests" ON)
if(DODOE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
