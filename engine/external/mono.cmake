set(DODOE_MONO_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mono" CACHE PATH "Path to embedded Mono runtime")

set(DODOE_MONO_INCLUDE_DIR "${DODOE_MONO_ROOT_DIR}/include/mono-2.0" CACHE PATH "Mono include dir" FORCE)
file(GLOB DODOE_MONO_LIBS CONFIGURE_DEPENDS "${DODOE_MONO_ROOT_DIR}/lib/*.lib")

if(NOT EXISTS "${DODOE_MONO_INCLUDE_DIR}")
	message(FATAL_ERROR "Mono include dir not found: ${DODOE_MONO_INCLUDE_DIR}")
endif()

if(NOT DODOE_MONO_LIBS)
	message(FATAL_ERROR "No .lib found in ${DODOE_MONO_ROOT_DIR}/lib")
endif()

if(NOT TARGET dodoe_mono)
	add_library(dodoe_mono INTERFACE)
	target_include_directories(dodoe_mono INTERFACE ${DODOE_MONO_INCLUDE_DIR})
	target_link_libraries(dodoe_mono INTERFACE ${DODOE_MONO_LIBS})
endif()

if(NOT TARGET mono::mono)
	add_library(mono::mono ALIAS dodoe_mono)
endif()

if(EXISTS "${DODOE_MONO_ROOT_DIR}/bin")
	if(NOT TARGET DodoeMonoRuntimeFiles)
		add_custom_target(DodoeMonoRuntimeFiles ALL
			COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"
			COMMAND ${CMAKE_COMMAND} -E copy_directory "${DODOE_MONO_ROOT_DIR}/bin" "${BINARY_ROOT_DIR}"
			COMMENT "Copy Mono runtime files to ${BINARY_ROOT_DIR}"
		)
		set_target_properties(DodoeMonoRuntimeFiles PROPERTIES FOLDER "ThirdParty/mono")
	endif()
endif()
