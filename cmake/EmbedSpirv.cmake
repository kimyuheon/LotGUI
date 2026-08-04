if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED VARIABLE)
    message(FATAL_ERROR "INPUT, OUTPUT, and VARIABLE are required")
endif()

file(READ "${INPUT}" _spirv_hex HEX)
string(LENGTH "${_spirv_hex}" _hex_length)
math(EXPR _byte_count "${_hex_length} / 2")
math(EXPR _remainder "${_byte_count} % 4")

if(NOT _remainder EQUAL 0)
    message(FATAL_ERROR "SPIR-V byte count must be divisible by four")
endif()

math(EXPR _word_count "${_byte_count} / 4")
math(EXPR _last_word "${_word_count} - 1")

set(_header "#pragma once\n\n#include <cstdint>\n\nnamespace lotui::generated {\n\ninline constexpr std::uint32_t ${VARIABLE}[] = {\n")

foreach(_word RANGE 0 ${_last_word})
    math(EXPR _offset "${_word} * 8")
    math(EXPR _offset_1 "${_offset} + 2")
    math(EXPR _offset_2 "${_offset} + 4")
    math(EXPR _offset_3 "${_offset} + 6")
    string(SUBSTRING "${_spirv_hex}" ${_offset} 2 _byte_0)
    string(SUBSTRING "${_spirv_hex}" ${_offset_1} 2 _byte_1)
    string(SUBSTRING "${_spirv_hex}" ${_offset_2} 2 _byte_2)
    string(SUBSTRING "${_spirv_hex}" ${_offset_3} 2 _byte_3)
    string(APPEND _header "    0x${_byte_3}${_byte_2}${_byte_1}${_byte_0}u,\n")
endforeach()

string(APPEND _header "};\n\n} // namespace lotui::generated\n")
file(WRITE "${OUTPUT}" "${_header}")
