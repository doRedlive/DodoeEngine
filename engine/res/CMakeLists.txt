find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)
find_program(DXC dxc)

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

set(HLSL_VS_PAIRS
	"sprite_pass_vs.hlsl sprite_pass.vert.dxil"
	"fullscreen_vs.hlsl fullscreen.vert.dxil"
	"main_camera_pass_vs.hlsl main_camera_pass.vert.dxil"
	"directional_light_shadow_pass_vs.hlsl directional_light_shadow_pass.vert.dxil"
	"imgui_pass_vs.hlsl imgui_pass.vert.dxil"
	"test_pass_vs.hlsl test_pass.vert.dxil"
	"ui_pass_vs.hlsl ui_pass.vert.dxil"
)

set(HLSL_PS_PAIRS
	"sprite_pass_ps.hlsl sprite_pass.frag.dxil"
	"sprite_pass_ps_traditional.hlsl sprite_pass_traditional.frag.dxil"
	"main_camera_pass_ps.hlsl main_camera_pass.frag.dxil"
	"main_camera_pass_nobindless_ps.hlsl main_camera_pass_nobindless.frag.dxil"
	"directional_light_shadow_pass_ps.hlsl directional_light_shadow_pass.frag.dxil"
	"deferred_light_pass_ps.hlsl deferred_light_pass.frag.dxil"
	"skybox_pass_ps.hlsl skybox_pass.frag.dxil"
	"tone_mapping_pass_ps.hlsl tone_mapping_pass.frag.dxil"
	"color_grading_pass_ps.hlsl color_grading_pass.frag.dxil"
	"fxaa_pass_ps.hlsl fxaa_pass.frag.dxil"
	"combine_pass_ps.hlsl combine_pass.frag.dxil"
	"imgui_pass_ps.hlsl imgui_pass.frag.dxil"
	"test_pass_ps.hlsl test_pass.frag.dxil"
	"ui_pass_ps.hlsl ui_pass.frag.dxil"
	"ui_pass_ps_array.hlsl ui_pass_array.frag.dxil"
)

set(HLSL_CS_PAIRS
	"gpu_culling_pass_cs.hlsl gpu_culling_pass.comp.dxil"
	"bucket_count_pass_cs.hlsl bucket_count_pass.comp.dxil"
	"bucket_fill_pass_cs.hlsl bucket_fill_pass.comp.dxil"
)

set(DXILS "")

if(DXC)
	foreach(PAIR ${HLSL_VS_PAIRS})
		string(REPLACE " " ";" PAIR_LIST ${PAIR})
		list(GET PAIR_LIST 0 SRC)
		list(GET PAIR_LIST 1 OUT)
		set(DXIL "${SHADER_OUTPUT_DIR}/${OUT}")
		add_custom_command(
			OUTPUT ${DXIL}
			COMMAND ${DXC} -T vs_6_0 -E main "${SHADER_SOURCE_DIR}/${SRC}" -Fo "${DXIL}"
			DEPENDS "${SHADER_SOURCE_DIR}/${SRC}"
			COMMENT "Compiling HLSL: ${SRC}"
			VERBATIM
		)
		list(APPEND DXILS ${DXIL})
	endforeach()

	foreach(PAIR ${HLSL_PS_PAIRS})
		string(REPLACE " " ";" PAIR_LIST ${PAIR})
		list(GET PAIR_LIST 0 SRC)
		list(GET PAIR_LIST 1 OUT)
		set(DXIL "${SHADER_OUTPUT_DIR}/${OUT}")
		add_custom_command(
			OUTPUT ${DXIL}
			COMMAND ${DXC} -T ps_6_0 -E main "${SHADER_SOURCE_DIR}/${SRC}" -Fo "${DXIL}"
			DEPENDS "${SHADER_SOURCE_DIR}/${SRC}"
			COMMENT "Compiling HLSL: ${SRC}"
			VERBATIM
		)
		list(APPEND DXILS ${DXIL})
	endforeach()

	foreach(PAIR ${HLSL_CS_PAIRS})
		string(REPLACE " " ";" PAIR_LIST ${PAIR})
		list(GET PAIR_LIST 0 SRC)
		list(GET PAIR_LIST 1 OUT)
		set(DXIL "${SHADER_OUTPUT_DIR}/${OUT}")
		add_custom_command(
			OUTPUT ${DXIL}
			COMMAND ${DXC} -T cs_6_0 -E main "${SHADER_SOURCE_DIR}/${SRC}" -Fo "${DXIL}"
			DEPENDS "${SHADER_SOURCE_DIR}/${SRC}"
			COMMENT "Compiling HLSL: ${SRC}"
			VERBATIM
		)
		list(APPEND DXILS ${DXIL})
	endforeach()
else()
	message(WARNING "DXC not found")
endif()

if(SPVS)
	add_custom_target(shaders ALL DEPENDS ${SPVS})
	set_target_properties(shaders PROPERTIES FOLDER "Engine/Resources")
endif()
if(DXILS)
	add_custom_target(shaders_dxil ALL DEPENDS ${DXILS})
	set_target_properties(shaders_dxil PROPERTIES FOLDER "Engine/Resources")
	if(TARGET shaders)
		add_dependencies(shaders shaders_dxil)
	endif()
endif()
