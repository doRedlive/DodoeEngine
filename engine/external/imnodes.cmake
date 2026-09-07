set(imnodes_SOURCE_DIR_ ${CMAKE_CURRENT_SOURCE_DIR}/imnodes)

add_library(imnodes STATIC
        ${imnodes_SOURCE_DIR_}/imnodes.cpp
        ${imnodes_SOURCE_DIR_}/imnodes.h
        ${imnodes_SOURCE_DIR_}/imnodes_internal.h
)

target_include_directories(imnodes PUBLIC
        $<BUILD_INTERFACE:${imnodes_SOURCE_DIR_}>
)

target_link_libraries(imnodes PUBLIC imgui)
