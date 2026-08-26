set(TARGET_NAME DodoeRuntime)

option(DODOE_ENABLE_PROFILING "Enable Chrome trace profiling" ON)

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

set(_DODOE_RUNTIME_HEADS "")
foreach(_header_file IN LISTS HEADER_FILES)
    file(RELATIVE_PATH _rel "${ENGINE_ROOT_DIR}/src" "${_header_file}")
    string(REPLACE "\\" "/" _rel "${_rel}")
    list(APPEND _DODOE_RUNTIME_HEADS "${_rel}")
endforeach()
list(REMOVE_DUPLICATES _DODOE_RUNTIME_HEADS)
set(DODOE_RUNTIME_HEADS "${_DODOE_RUNTIME_HEADS}" CACHE STRING "Runtime headers for DodoePreCompile" FORCE)

add_library(${TARGET_NAME} SHARED ${DODOE_ALL_SOURCES})

target_sources(${TARGET_NAME} PRIVATE
    ${ENGINE_ROOT_DIR}/src/_generated/script/py_bindings_components.generated.cpp
)
target_compile_definitions(${TARGET_NAME} PRIVATE DODOE_BUILD_DLL)

include(${ENGINE_ROOT_DIR}/scripts/cmake/DodoeBigObj.cmake)

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
                ${ENGINE_ROOT_DIR}/external/JoltPhysics
                ${ENGINE_ROOT_DIR}/external/vulkanmemoryallocator/include
                ${ENGINE_ROOT_DIR}/external/json/include
                ${ENGINE_ROOT_DIR}/external/assimp/include
                ${ENGINE_ROOT_DIR}/external/cutie-rhi/include
                ${ENGINE_ROOT_DIR}/external/miniaudio
                ${Vulkan_INCLUDE_DIRS}
        PRIVATE
                ${ENGINE_ROOT_DIR}/src/runtime
)

target_compile_definitions(${TARGET_NAME} PUBLIC $<$<CONFIG:Debug>:DODOE_DEBUG_ENABLED>)
target_compile_definitions(${TARGET_NAME} PUBLIC $<$<BOOL:${DODOE_ENABLE_PROFILING}>:DODOE_PROFILE_ENABLED=1>)
if(TARGET DodoeProfiling)
    target_link_libraries(${TARGET_NAME} PUBLIC DodoeProfiling)
endif()
if(DODOE_EDITOR)
    target_compile_definitions(${TARGET_NAME} PUBLIC DODOE_EDITOR_ENABLED)
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
        PUBLIC Jolt
        PUBLIC assimp
        PUBLIC cutie
        PUBLIC cutie_vk
        PUBLIC cutie_gl
        PUBLIC cutie_d3d12
        PUBLIC ${Vulkan_LIBRARIES}
        PUBLIC d3d12
        PUBLIC dxgi
        PUBLIC mimalloc::mimalloc
        PUBLIC spirv-cross-core
        PUBLIC spirv-cross-glsl
        PUBLIC spirv-cross-reflect
        PUBLIC audio
)

if(DODOE_PYTHON_ENABLED)
    target_link_libraries(${TARGET_NAME} PUBLIC pybind11::embed)
    target_link_directories(${TARGET_NAME} PRIVATE "${ENGINE_ROOT_DIR}/external/python/libs")
    target_compile_definitions(${TARGET_NAME} PUBLIC DODOE_PYTHON_ENABLED)
endif()

if(TARGET nethost::nethost)
    target_link_libraries(${TARGET_NAME} PUBLIC nethost::nethost)
endif()

target_compile_definitions(${TARGET_NAME}
        PUBLIC GLFW_INCLUDE_NONE
        PUBLIC NOMINMAX
        PUBLIC WIN32_LEAN_AND_MEAN
        PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        PUBLIC $<$<CONFIG:Debug>:ENTT_DISABLE_ASSERT>
        PRIVATE $<$<CONFIG:Debug>:DO_DEBUG>
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${TARGET_NAME}>
        "${BINARY_ROOT_DIR}/"
    COMMENT "Copy DodoeRuntime.dll -> ${BINARY_ROOT_DIR}/"
)

if(NETHOST_DLL)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${NETHOST_DLL}"
            "${BINARY_ROOT_DIR}/"
        COMMENT "Copy nethost.dll -> ${BINARY_ROOT_DIR}/"
    )
endif()

if(DODOE_PYTHON_ENABLED)
    add_dependencies(${TARGET_NAME} DodoePythonRuntimeFiles)
endif()

dodoe_set_folder(${TARGET_NAME} "Engine")
