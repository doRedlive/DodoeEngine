set(TARGET_NAME Cakery)

file(GLOB_RECURSE CAKERY_SOURCES
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/cakery/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cakery/*.c
)

file(GLOB_RECURSE CAKERY_HEADERS
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/cakery/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cakery/*.h
)

set(CAKERY_ALL_SOURCES ${CAKERY_SOURCES} ${CAKERY_HEADERS})


add_executable(Cakery EXCLUDE_FROM_ALL ${CAKERY_ALL_SOURCES})

target_link_libraries(Cakery PRIVATE DodoeRuntime)

target_include_directories(Cakery
    PRIVATE
        ${ENGINE_ROOT_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:${TARGET_NAME}>/" "${BINARY_ROOT_DIR}"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        ${POST_BUILD_COMMANDS}
)

