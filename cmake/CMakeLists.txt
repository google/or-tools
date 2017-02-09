
#Connecting the "dependencies" directory with the rest of the project

SET_PROPERTY(GLOBAL PROPERTY EP_BASE "${OR_TOOLS_SOURCE_DIR}/dependencies")
GET_PROPERTY(V_EP_BASE GLOBAL PROPERTY EP_BASE)

SET(CMAKE_INSTALL_PREFIX ${V_EP_BASE} CACHE INTERNAL "" FORCE)

SET(DEPENDENCIES_SOURCES ${V_EP_BASE}/Source CACHE INTERNAL "" FORCE)

SET(DEPENDENCIES_INSTALL ${V_EP_BASE}/Install CACHE INTERNAL "" FORCE)

SET(DEPENDENCIES_ARCHIVES ${V_EP_BASE}/Archives CACHE INTERNAL "" FORCE)

SET(DEPENDENCIES_INCLUDE ${V_EP_BASE}/include CACHE INTERNAL "" FORCE)

SET(DEPENDENCIES_LIB ${V_EP_BASE}/lib CACHE INTERNAL "" FORCE)

SET(MAKEFILE_LOCAL "${OR_TOOLS_SOURCE_DIR}/Makefile.local")

#Using dependencies/Install/bin && dependencies/Install/lib
SET(CMAKE_PREFIX_PATH "${CMAKE_INSTALL_PREFIX}" CACHE INTERNAL "" FORCE)

#########>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<###########

#[[
Define the or-tools directories.
]]

SET(REL_SRC_DIR "src" CACHE INTERNAL "Relative or-tools sources directory" FORCE)
SET(SRC_DIR "${OR_TOOLS_SOURCE_DIR}/${REL_SRC_DIR}" CACHE INTERNAL "or-tools sources directory" FORCE)

SET(REL_GEN_DIR "src/gen" CACHE INTERNAL "Relative or-tools generated sources directory" FORCE)
SET(GEN_DIR "${OR_TOOLS_SOURCE_DIR}/${REL_GEN_DIR}" CACHE INTERNAL "or-tools generated sources directory" FORCE)
file(MAKE_DIRECTORY ${GEN_DIR})

SET(REL_EXAMPLES_DIR "examples" CACHE INTERNAL "Relative or-tools examples directory" FORCE)
SET(EXAMPLES_DIR "${OR_TOOLS_SOURCE_DIR}/${REL_EXAMPLES_DIR}" CACHE INTERNAL "or-tools examples directory" FORCE)

SET(REL_CPP_EXAMPLES_DIR "examples/cpp" CACHE INTERNAL "Relative or-tools C++ examples directory" FORCE)
SET(CPP_EXAMPLES_DIR "${OR_TOOLS_SOURCE_DIR}/${REL_CPP_EXAMPLES_DIR}" CACHE INTERNAL "or-tools C++ examples directory" FORCE)

SET(REL_CS_EXAMPLES_DIR "examples/chsarp" CACHE INTERNAL "Relative or-tools C# examples directory" FORCE)
SET(CS_EXAMPLES_DIR "${OR_TOOLS_SOURCE_DIR}/${REL_CS_EXAMPLES_DIR}" CACHE INTERNAL "or-tools C# examples directory" FORCE)

SET(REL_PYTHON_EXAMPLES_DIR "examples/python" CACHE INTERNAL "Relative or-tools python examples directory" FORCE)
SET(PYTHON_EXAMPLES_DIR "${OR_TOOLS_SOURCE_DIR}/${REL_PYTHON_EXAMPLES_DIR}" CACHE INTERNAL "or-tools python examples directory" FORCE)

SET(REL_FZ_EXAMPLES_DIR "examples/flatzinc" CACHE INTERNAL "Relative or-tools flatzinc examples directory" FORCE)
SET(FZ_EXAMPLES_DIR "${OR_TOOLS_SOURCE_DIR}/${REL_FZ_EXAMPLES_DIR}" CACHE INTERNAL "or-tools flatzinc examples directory" FORCE)

SET(REL_LIB_DIR "lib" CACHE INTERNAL "Relative or-tools lib directory" FORCE)
SET(LIB_DIR "${OR_TOOLS_SOURCE_DIR}/${REL_LIB_DIR}" CACHE INTERNAL "or-tools lib directory" FORCE)
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${LIB_DIR}" CACHE INTERNAL "or-tools lib directory" FORCE)

SET (REL_BIN_DIR "bin" CACHE INTERNAL "Relative or-tools bin directory" FORCE)
SET (BIN_DIR "${OR_TOOLS_SOURCE_DIR}/${REL_BIN_DIR}" CACHE INTERNAL "or-tools bin directory" FORCE)
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}" CACHE INTERNAL "or-tools bin directory" FORCE)
