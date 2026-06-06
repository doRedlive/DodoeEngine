# Cakery editor — Avalonia .NET project
# The editor executable and all its dependencies are published to ${BINARY_ROOT_DIR} (bin/).
# DodoeRuntime.dll is also copied there by the runtime CMakeLists, so the editor can find it.

find_program(DOTNET_EXECUTABLE dotnet REQUIRED)

set(CAKERY_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Cakery)
set(CAKERY_PROJECT_FILE ${CAKERY_PROJECT_DIR}/Cakery.csproj)

set(CAKERY_OUTPUT_EXE ${BINARY_ROOT_DIR}/Cakery.exe)

file(GLOB_RECURSE CAKERY_SOURCES
    CONFIGURE_DEPENDS
    ${CAKERY_PROJECT_DIR}/*.cs
    ${CAKERY_PROJECT_DIR}/*.axaml
)

add_custom_command(
    OUTPUT ${CAKERY_OUTPUT_EXE}
    DEPENDS ${CAKERY_SOURCES} ${CAKERY_PROJECT_FILE}
    COMMAND ${DOTNET_EXECUTABLE} publish
        ${CAKERY_PROJECT_FILE}
        -c $<CONFIG>
        --no-self-contained
        -o ${BINARY_ROOT_DIR}
    WORKING_DIRECTORY ${CAKERY_PROJECT_DIR}
    COMMENT "Publishing Cakery ($<CONFIG>) -> ${BINARY_ROOT_DIR}"
)

add_custom_target(Cakery ALL
    DEPENDS ${CAKERY_OUTPUT_EXE}
)

add_dependencies(Cakery DodoeRuntime)
