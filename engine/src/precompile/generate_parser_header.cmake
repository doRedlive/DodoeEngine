if(NOT DEFINED INPUT_LIST_FILE)
    message(FATAL_ERROR "INPUT_LIST_FILE is not defined")
endif()

if(NOT DEFINED OUTPUT_HEADER_FILE)
    message(FATAL_ERROR "OUTPUT_HEADER_FILE is not defined")
endif()

file(READ "${INPUT_LIST_FILE}" _content)
string(REPLACE "\r" "" _content "${_content}")
string(REPLACE "\n" "" _content "${_content}")

set(_headers "${_content}")

file(WRITE "${OUTPUT_HEADER_FILE}" "#ifndef __PARSER_HEADER_H__\n")
file(APPEND "${OUTPUT_HEADER_FILE}" "#define __PARSER_HEADER_H__\n\n")

foreach(_h IN LISTS _headers)
    if(_h STREQUAL "")
        continue()
    endif()
    string(REPLACE "\\" "/" _h "${_h}")
    file(APPEND "${OUTPUT_HEADER_FILE}" "#include \"${_h}\"\n")
endforeach()

file(APPEND "${OUTPUT_HEADER_FILE}" "\n#endif\n")

