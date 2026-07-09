set(TARGET_NAME DodoeRuntime)

find_package(Vulkan REQUIRED)
if (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found!")
endif()

file(GLOB_RECURSE SOURCE_FILES
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
)

file(GLOB_RECURSE HEADER_FILES
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

set(DODOE_ALL_SOURCES ${SOURCE_FILES} ${HEADER_FILES})

# bindings/ directory removed — editor links directly to runtime API

set(_DODOE_RUNTIME_HEADS "")
foreach(_header_file IN LISTS HEADER_FILES)
    file(RELATIVE_PATH _rel "${ENGINE_ROOT_DIR}/src" "${_header_file}")
    string(REPLACE "\\" "/" _rel "${_rel}")
    list(APPEND _DODOE_RUNTIME_HEADS "${_rel}")
endforeach()
list(REMOVE_DUPLICATES _DODOE_RUNTIME_HEADS)
set(DODOE_RUNTIME_HEADS "${_DODOE_RUNTIME_HEADS}" CACHE STRING "Runtime headers for DodoePreCompile" FORCE)

add_library(${TARGET_NAME} SHARED ${DODOE_ALL_SOURCES})
target_compile_definitions(${TARGET_NAME} PRIVATE DODOE_BUILD_DLL)

source_group(TREE ${ENGINE_ROOT_DIR}/src FILES ${DODOE_ALL_SOURCES})

target_include_directories(${TARGET_NAME}
        PUBLIC
                $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/src>
                $<INSTALL_INTERFACE:include/${TARGET_NAME}-${PROJECT_VERSION}>
                ${ENGINE_ROOT_DIR}/external/spdlog/include
                ${ENGINE_ROOT_DIR}/external/glfw/include
                ${ENGINE_ROOT_DIR}/external/dear-imgui
                ${ENGINE_ROOT_DIR}/external/stb_image
                ${ENGINE_ROOT_DIR}/external/glm
                ${ENGINE_ROOT_DIR}/external/entt/src
                ${ENGINE_ROOT_DIR}/external/box2d/include
                ${ENGINE_ROOT_DIR}/external/vulkanmemoryallocator/include
                ${ENGINE_ROOT_DIR}/external/json/include
                ${ENGINE_ROOT_DIR}/external/assimp/include
                ${ENGINE_ROOT_DIR}/external/cutie-rhi/include
                ${Vulkan_INCLUDE_DIRS}
        PRIVATE
                ${ENGINE_ROOT_DIR}/src/runtime
)

if(DODOE_EDITOR)
    target_compile_definitions(${TARGET_NAME} PRIVATE DODOE_EDITOR)
endif()
target_precompile_headers(${TARGET_NAME} PUBLIC dopch.h)

target_link_libraries(${TARGET_NAME}
        PUBLIC glad
        PUBLIC glfw
        PUBLIC glm
        PUBLIC spdlog::spdlog
        PUBLIC stb_image
        PUBLIC imgui
        PUBLIC EnTT::EnTT
        PUBLIC box2d
        PUBLIC assimp
        PUBLIC cutie
        PUBLIC cutie_vk
        PUBLIC cutie_gl
        PUBLIC cutie_d3d12
        PUBLIC ${Vulkan_LIBRARIES}
        PUBLIC d3d12
        PUBLIC dxgi
)

if(DODOE_PYTHON_ENABLED)
    target_link_libraries(${TARGET_NAME} PUBLIC pybind11::embed)
    target_link_directories(${TARGET_NAME} PRIVATE "${ENGINE_ROOT_DIR}/external/python/libs")
    target_compile_definitions(${TARGET_NAME} PUBLIC DODOE_PYTHON_ENABLED)
endif()

if(TARGET mono::mono)
    target_link_libraries(${TARGET_NAME} PUBLIC mono::mono)
endif()

if(TARGET mono::mono)
    target_link_options(${TARGET_NAME} PRIVATE /DELAYLOAD:coreclr.dll)
endif()

target_compile_definitions(${TARGET_NAME}
        PUBLIC GLFW_INCLUDE_NONE
        PUBLIC NOMINMAX
        PUBLIC WIN32_LEAN_AND_MEAN
        PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        PUBLIC $<$<CONFIG:Debug>:ENTT_DISABLE_ASSERT>
        PRIVATE $<$<CONFIG:Debug>:DO_DEBUG>
)

if(DODOE_EDITOR)
    add_dependencies(${TARGET_NAME} DodoeMonoRuntimeFiles)
    if(DODOE_PYTHON_ENABLED)
        add_dependencies(${TARGET_NAME} DodoePythonRuntimeFiles)
    endif()

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${TARGET_NAME}>
            "${BINARY_ROOT_DIR}/"
        COMMENT "Copy DodoeRuntime.dll -> ${BINARY_ROOT_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ENGINE_ROOT_DIR}/external/mono/bin/coreclr.dll"
            "${BINARY_ROOT_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ENGINE_ROOT_DIR}/external/mono/bin/System.Private.CoreLib.dll"
            "${BINARY_ROOT_DIR}/"
    )
endif()
