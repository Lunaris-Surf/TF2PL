# Embeds a text file as a list of C++ raw string literal chunks, for use as an array initializer:
#   constexpr const char* CHUNKS[] = {
#   #include "generated.inl"
#   };
# Chunks keep each literal well under MSVC's ~16 KB string literal limit. The input must be ASCII
# and must not contain the sequence )tf2plweb"
#
# usage: cmake -DINPUT=<file> -DOUTPUT=<file> -P EmbedFile.cmake

file(READ "${INPUT}" content)
string(FIND "${content}" ")tf2plweb\"" bad)
if (NOT bad EQUAL -1)
	message(FATAL_ERROR "${INPUT} contains the raw string delimiter )tf2plweb\"")
endif()

string(LENGTH "${content}" length)
set(out "// generated from ${INPUT} by EmbedFile.cmake - do not edit\n")
set(pos 0)
while (pos LESS length)
	string(SUBSTRING "${content}" ${pos} 4000 chunk)
	string(APPEND out "R\"tf2plweb(${chunk})tf2plweb\",\n")
	math(EXPR pos "${pos} + 4000")
endwhile()

file(WRITE "${OUTPUT}.tmp" "${out}")
# only touch the output when it changed, so rebuilding doesn't recompile WebUI.cpp needlessly
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OUTPUT}.tmp" "${OUTPUT}")
file(REMOVE "${OUTPUT}.tmp")
