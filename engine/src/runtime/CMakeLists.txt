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

add_library(${TARGET_NAME} STATIC ${DODOE_ALL_SOURCES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${DODOE_ALL_SOURCES})

target_include_directories(${TARGET_NAME}
        PUBLIC
                $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/src/runtime>
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
                ${ENGINE_ROOT_DIR}/external/nvrhi/include
                ${ENGINE_ROOT_DIR}/external/nvrhi/thirdparty/Vulkan-Headers/include
                ${Vulkan_INCLUDE_DIRS}
        PRIVATE
                ${ENGINE_ROOT_DIR}/src
)

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
        PUBLIC sol2::sol2
        PUBLIC lua_static
        PUBLIC nvrhi
        PUBLIC nvrhi_vk
        PUBLIC ${Vulkan_LIBRARIES}
)

target_compile_definitions(${TARGET_NAME}
        PUBLIC GLFW_INCLUDE_NONE
        PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        PUBLIC $<$<CONFIG:Debug>:NDEBUG>
        PRIVATE $<$<CONFIG:Debug>:DO_DEBUG>
)
