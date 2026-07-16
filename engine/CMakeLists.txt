set(ENGINE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
include(${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmake/DodoeCommon.cmake)

option(DODOE_EDITOR "Build the Cakery editor" ON)

add_subdirectory(external)

add_subdirectory(res)

add_subdirectory(src/runtime)
if(DODOE_EDITOR)
	add_subdirectory(src/editor)
endif()
add_subdirectory(src/sandbox)
add_subdirectory(src/metaparser)
add_subdirectory(src/scriptcore)

if(TARGET shaders)
	add_dependencies(DodoeRuntime shaders)
endif()

set(CODEGEN_TARGET "DodoePreCompile")
include(src/precompile/precompile.cmake)
dodoe_set_folder("${CODEGEN_TARGET}" "Engine/Tools")

add_dependencies(DodoeRuntime "${CODEGEN_TARGET}")
add_dependencies("${CODEGEN_TARGET}" "DodoeParser")

if(TARGET GreenCakeBuild)
    add_dependencies(DodoeRuntime GreenCakeBuild)
endif()
