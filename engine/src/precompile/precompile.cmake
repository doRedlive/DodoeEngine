# 
set(PRECOMPILE_TOOLS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(DODOE_PRECOMPILE_PARAMS_PATH "${PRECOMPILE_TOOLS_PATH}/precompile.json")

if (NOT DEFINED DODOE_RUNTIME_HEADS)
    set(DODOE_RUNTIME_HEADS "")
endif()
if (NOT DEFINED DODOE_EDITOR_HEADS)
    set(DODOE_EDITOR_HEADS "")
endif()

set(_DODOE_PRECOMPILE_HEADS "")
foreach(_h IN LISTS DODOE_RUNTIME_HEADS DODOE_EDITOR_HEADS)
    if (NOT _h STREQUAL "")
        list(APPEND _DODOE_PRECOMPILE_HEADS "${_h}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _DODOE_PRECOMPILE_HEADS)
list(JOIN _DODOE_PRECOMPILE_HEADS ";" _DODOE_PRECOMPILE_HEADS_JOINED)
file(WRITE "${DODOE_PRECOMPILE_PARAMS_PATH}" "${_DODOE_PRECOMPILE_HEADS_JOINED}")

#
# use wine for linux
if (CMAKE_HOST_WIN32)
    set(PRECOMPILE_PRE_EXE)
	set(PRECOMPILE_PARSER ${PRECOMPILE_TOOLS_PATH}/DodoeParser.exe)
    set(sys_include "*") 
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux" )
    set(PRECOMPILE_PRE_EXE)
	set(PRECOMPILE_PARSER ${PRECOMPILE_TOOLS_PATH}/DodoeParser)
    set(sys_include "/usr/include/c++/9/") 
    #execute_process(COMMAND chmod a+x ${PRECOMPILE_PARSER} WORKING_DIRECTORY ${PRECOMPILE_TOOLS_PATH})
elseif(CMAKE_HOST_APPLE)
    find_program(XCRUN_EXECUTABLE xcrun)
    if(NOT XCRUN_EXECUTABLE)
      message(FATAL_ERROR "xcrun not found!!!")
    endif()

    execute_process(
      COMMAND ${XCRUN_EXECUTABLE} --sdk macosx --show-sdk-platform-path
      OUTPUT_VARIABLE osx_sdk_platform_path_test
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(PRECOMPILE_PRE_EXE)
	set(PRECOMPILE_PARSER ${PRECOMPILE_TOOLS_PATH}/DodoeParser)
    set(sys_include "${osx_sdk_platform_path_test}/../../Toolchains/XcodeDefault.xctoolchain/usr/include/c++/v1") 
endif()

set (PARSER_INPUT ${CMAKE_BINARY_DIR}/parser_header.h)
### BUILDING ====================================================================================
set(PRECOMPILE_TARGET "DodoePreCompile")

# Called first time when building target 
add_custom_target(${PRECOMPILE_TARGET} ALL

# COMMAND # (DEBUG: DON'T USE )
#     this will make configure_file() is called on each compile
#   ${CMAKE_COMMAND} -E touch ${PRECOMPILE_PARAM_IN_PATH}a

COMMAND
  ${CMAKE_COMMAND} -DINPUT_LIST_FILE="${DODOE_PRECOMPILE_PARAMS_PATH}" -DOUTPUT_HEADER_FILE="${PARSER_INPUT}" -P "${CMAKE_CURRENT_SOURCE_DIR}/src/precompile/generate_parser_header.cmake"
COMMAND
  ${PRECOMPILE_PARSER} "*"  "${PARSER_INPUT}"  "${ENGINE_ROOT_DIR}/src" ${sys_include} "DODOE" 0
)
