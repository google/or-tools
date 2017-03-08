include(CMakeParseArguments)

function(PREFIX_SUFFIX)
    cmake_parse_arguments(
        PARSED # prefix of output variables
        ""
        "PREFIX;SUFFIX;RESULT" # list of names of mono-valued arguments
        "LIST" # list of names of multi-valued arguments (output variables are lists)
        ${ARGN} # arguments of the function to parse, here we take the all original ones
    )
    set(${PARSED_RESULT} "")
    foreach(val ${PARSED_LIST})
    	list(APPEND ${PARSED_RESULT} "${PARSED_PREFIX}${val}${PARSED_SUFFIX}")
    endforeach()
    set(${PARSED_RESULT} ${${PARSED_RESULT}} PARENT_SCOPE)
endfunction()

#[[
Test PREFIX_SUFFIX

PREFIX_SUFFIX(
	PREFIX mo
	SUFFIX mi
	LIST ba bo bi
	RESULT var
)

message("${var}")
]]