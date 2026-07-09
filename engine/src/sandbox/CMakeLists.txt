set(TARGET_NAME DodoeSandbox)

file(GLOB_RECURSE SANDBOX_SOURCES
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/sandbox/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sandbox/*.c
)

file(GLOB_RECURSE SANDBOX_HEADERS
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/sandbox/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sandbox/*.h
)

set(SANDBOX_ALL_SOURCES ${SANDBOX_SOURCES} ${SANDBOX_HEADERS})

add_executable(${TARGET_NAME} ${SANDBOX_ALL_SOURCES})

target_link_libraries(${TARGET_NAME}
        PRIVATE DodoeRuntime
)

if(TARGET GreenCake)
    add_dependencies(${TARGET_NAME} GreenCake)
endif()

target_include_directories(${TARGET_NAME}
        PRIVATE
                ${ENGINE_ROOT_DIR}/src
                ${CMAKE_CURRENT_SOURCE_DIR}
)

set(POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${TARGET_NAME}>" "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:${TARGET_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:DodoeRuntime>" "${BINARY_ROOT_DIR}/"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        ${POST_BUILD_COMMANDS}
)