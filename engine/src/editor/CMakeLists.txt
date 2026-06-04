set(TARGET_NAME Cakery)

file(GLOB_RECURSE CAKERY_SOURCES
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/cakery/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cakery/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/cakery/**/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cakery/**/*.c
)

file(GLOB_RECURSE CAKERY_HEADERS
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/cakery/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cakery/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/cakery/**/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cakery/**/*.h
)

set(CAKERY_ALL_SOURCES ${CAKERY_SOURCES} ${CAKERY_HEADERS})

set(_DODOE_EDITOR_HEADS "")
foreach(_header_file IN LISTS CAKERY_HEADERS)
    file(RELATIVE_PATH _rel "${ENGINE_ROOT_DIR}/src" "${_header_file}")
    string(REPLACE "\\" "/" _rel "${_rel}")
    list(APPEND _DODOE_EDITOR_HEADS "${_rel}")
endforeach()
list(REMOVE_DUPLICATES _DODOE_EDITOR_HEADS)
set(DODOE_EDITOR_HEADS "${_DODOE_EDITOR_HEADS}" CACHE STRING "Editor headers for DodoePreCompile" FORCE)


add_executable(Cakery EXCLUDE_FROM_ALL ${CAKERY_ALL_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/native_api/dodoe_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/dodoe_wrap.cxx
)

target_link_libraries(Cakery PRIVATE DodoeRuntime)
target_compile_definitions(Cakery PRIVATE DODOE_EDITOR DODOE_BUILD_DLL)
target_compile_definitions(DodoeRuntime PRIVATE DODOE_EDITOR)

if(TARGET GreenCake)
    add_dependencies(Cakery GreenCake)
endif()

target_include_directories(Cakery
    PRIVATE
        ${ENGINE_ROOT_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${TARGET_NAME}>" "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:${TARGET_NAME}>"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        ${POST_BUILD_COMMANDS}
)


