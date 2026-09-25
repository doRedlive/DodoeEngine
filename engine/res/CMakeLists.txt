find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)
find_program(SPIRV_CROSS spirv-cross REQUIRED)
find_program(DXC dxc)

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/bin")
set(SHADER_HLSL_TMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/shader_hlsl")

file(GLOB_RECURSE SHADERS CONFIGURE_DEPENDS
	"${SHADER_SOURCE_DIR}/*.vert"
	"${SHADER_SOURCE_DIR}/*.frag"
	"${SHADER_SOURCE_DIR}/*.geom"
	"${SHADER_SOURCE_DIR}/*.comp"
)

file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${SHADER_HLSL_TMP_DIR}")

set(ARTIFACTS "")

foreach(SHADER ${SHADERS})
	get_filename_component(NAME ${SHADER} NAME)
	file(RELATIVE_PATH REL_PATH "${SHADER_SOURCE_DIR}" "${SHADER}")
	get_filename_component(REL_DIR "${REL_PATH}" DIRECTORY)
	if(REL_DIR)
		file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}/${REL_DIR}")
		file(MAKE_DIRECTORY "${SHADER_HLSL_TMP_DIR}/${REL_DIR}")
	endif()
	set(SPV "${SHADER_OUTPUT_DIR}/${REL_PATH}.spv")
	set(HLSL "${SHADER_HLSL_TMP_DIR}/${REL_PATH}.hlsl")
	set(DXIL "${SHADER_OUTPUT_DIR}/${REL_PATH}.dxil")

	if(NAME MATCHES "\\.vert$")
		set(SC_STAGE vert)
		set(DXC_TARGET vs_6_0)
	elseif(NAME MATCHES "\\.frag$")
		set(SC_STAGE frag)
		set(DXC_TARGET ps_6_0)
	elseif(NAME MATCHES "\\.geom$")
		set(SC_STAGE geom)
		set(DXC_TARGET gs_6_0)
	elseif(NAME MATCHES "\\.comp$")
		set(SC_STAGE comp)
		set(DXC_TARGET cs_6_0)
	endif()

	add_custom_command(
		OUTPUT ${SPV}
		COMMAND ${GLSLANG_VALIDATOR} -V "-I${SHADER_SOURCE_DIR}" "-P#extension GL_GOOGLE_include_directive : enable" "${SHADER}" -o "${SPV}"
		DEPENDS "${SHADER}" "${SHADER_SOURCE_DIR}/common/shader_parameter_sets.glsl"
		COMMENT "GLSL->SPIR-V: ${NAME}"
		VERBATIM
	)
	add_custom_command(
		OUTPUT ${HLSL}
		COMMAND ${SPIRV_CROSS} --hlsl --shader-model 60 --stage ${SC_STAGE} "${SPV}" --output "${HLSL}"
		DEPENDS ${SPV}
		COMMENT "SPIR-V->HLSL: ${NAME}"
		VERBATIM
	)
	list(APPEND ARTIFACTS ${SPV})
	if(DXC)
		add_custom_command(
			OUTPUT ${DXIL}
			COMMAND ${DXC} -T ${DXC_TARGET} -E main "${HLSL}" -Fo "${DXIL}"
			DEPENDS ${HLSL}
			COMMENT "HLSL->DXIL: ${NAME}"
			VERBATIM
		)
		list(APPEND ARTIFACTS ${DXIL})
	endif()
endforeach()

set(DXILS "")

if(ARTIFACTS)
	add_custom_target(shaders ALL DEPENDS ${ARTIFACTS})
	set_target_properties(shaders PROPERTIES FOLDER "Engine/Resources")
endif()
