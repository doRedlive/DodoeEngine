set(ADS_VERSION "4.0.0" CACHE STRING "ADS version override (skip git)")

if(NOT TARGET ads::qtadvanceddocking-qt6)
    add_subdirectory(qt-ads ${CMAKE_BINARY_DIR}/qt-ads)
endif()
