find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/bin")

file(GLOB SHADERS CONFIGURE_DEPENDS
	"${SHADER_SOURCE_DIR}/*.vert"
	"${SHADER_SOURCE_DIR}/*.frag"
	"${SHADER_SOURCE_DIR}/*.geom"
)

file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

set(SPVS "")
foreach(SHADER ${SHADERS})
	get_filename_component(NAME ${SHADER} NAME)
	set(SPV "${SHADER_OUTPUT_DIR}/${NAME}.spv")

	add_custom_command(
		OUTPUT ${SPV}
		COMMAND ${GLSLANG_VALIDATOR} -V "${SHADER}" -o "${SPV}"
		DEPENDS ${SHADER}
		COMMENT "Compiling shader: ${NAME}"
		VERBATIM
	)

	list(APPEND SPVS ${SPV})
endforeach()

if(SPVS)
	add_custom_target(shaders ALL DEPENDS ${SPVS})
endif()