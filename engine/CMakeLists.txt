set(ENGINE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

option(DODOE_EDITOR "Build the Cakery editor" OFF)

add_subdirectory(external)

if(DEFINED DODOE_MONO_INCLUDE_DIR AND EXISTS "${DODOE_MONO_INCLUDE_DIR}")
	include_directories("${DODOE_MONO_INCLUDE_DIR}")
endif()

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
set_target_properties("${CODEGEN_TARGET}" PROPERTIES FOLDER "Engine")

add_dependencies(DodoeRuntime "${CODEGEN_TARGET}")
add_dependencies("${CODEGEN_TARGET}" "DodoeParser")

if(TARGET GreenCake)
    add_dependencies(DodoeRuntime GreenCake)
endif()
