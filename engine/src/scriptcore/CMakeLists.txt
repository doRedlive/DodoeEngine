find_program(DOTNET_EXECUTABLE dotnet REQUIRED)

set(CS_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CS_PROJECT_FILE ${CS_PROJECT_DIR}/GreenCake.csproj)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(DOTNET_CONFIG Release)
else()
    set(DOTNET_CONFIG Debug)
endif()
set(TARGET_COPY_DIR ${BINARY_ROOT_DIR})

set(DOTNET_OUT_DIR ${CS_PROJECT_DIR}/bin/${DOTNET_CONFIG}/net10.0)
set(CS_OUTPUT_DLL ${DOTNET_OUT_DIR}/GreenCake.dll)
set(CS_OUTPUT_DEPS ${DOTNET_OUT_DIR}/GreenCake.deps.json)
set(CS_OUTPUT_RUNTIMECONFIG ${DOTNET_OUT_DIR}/GreenCake.runtimeconfig.json)

file(GLOB_RECURSE CS_SOURCES
    CONFIGURE_DEPENDS
    ${CS_PROJECT_DIR}/Source/*.cs
)

include_external_msproject(GreenCake ${CS_PROJECT_FILE}
    TYPE FAE04EC0-301F-11D3-BF4B-00C04F79EFBC
    PLATFORM AnyCPU
)
set_target_properties(GreenCake PROPERTIES FOLDER "ScriptCore")

add_custom_command(
    OUTPUT ${CS_OUTPUT_DLL}
    DEPENDS ${CS_SOURCES} ${CS_PROJECT_FILE}
    COMMAND ${DOTNET_EXECUTABLE} build
        ${CS_PROJECT_FILE}
        -c ${DOTNET_CONFIG}
        --no-self-contained
    WORKING_DIRECTORY ${CS_PROJECT_DIR}
    COMMENT "Building GreenCake.dll (${DOTNET_CONFIG})"
)

add_custom_command(
    OUTPUT ${TARGET_COPY_DIR}/GreenCake.dll
    DEPENDS ${CS_OUTPUT_DLL} ${CS_OUTPUT_DEPS} ${CS_OUTPUT_RUNTIMECONFIG}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TARGET_COPY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CS_OUTPUT_DLL}
            ${TARGET_COPY_DIR}/GreenCake.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CS_OUTPUT_DEPS}
            ${TARGET_COPY_DIR}/GreenCake.deps.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CS_OUTPUT_RUNTIMECONFIG}
            ${TARGET_COPY_DIR}/GreenCake.runtimeconfig.json
    COMMENT "Copy GreenCake.dll -> ${TARGET_COPY_DIR}"
)

add_custom_command(
    OUTPUT ${TARGET_COPY_DIR}/DodoeRuntime.runtimeconfig.json
    DEPENDS ${TARGET_COPY_DIR}/GreenCake.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${TARGET_COPY_DIR}/GreenCake.runtimeconfig.json
            ${TARGET_COPY_DIR}/DodoeRuntime.runtimeconfig.json
    COMMENT "Copy GreenCake.runtimeconfig.json -> DodoeRuntime.runtimeconfig.json"
)

add_custom_target(GreenCakeBuild ALL
    DEPENDS ${TARGET_COPY_DIR}/GreenCake.dll
    DEPENDS ${TARGET_COPY_DIR}/DodoeRuntime.runtimeconfig.json
)
set_target_properties(GreenCakeBuild PROPERTIES FOLDER "ScriptCore")
add_dependencies(GreenCake GreenCakeBuild)
