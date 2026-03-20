set(ENGINE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(external)

add_subdirectory(src/runtime)
add_subdirectory(src/sandbox)
add_subdirectory(src/editor)
add_subdirectory(src/metaparser)

set(CODEGEN_TARGET "DodoePreCompile")
include(src/precompile/precompile.cmake)
set_target_properties("${CODEGEN_TARGET}" PROPERTIES FOLDER "Engine")

add_dependencies(DodoeRuntime "${CODEGEN_TARGET}")
add_dependencies("${CODEGEN_TARGET}" "DodoeParser")