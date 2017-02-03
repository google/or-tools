set(OR_TOOLS_ROOT_DIR "${OR_TOOLS_SOURCE_DIR}/.." CACHE INTERNAL "or-tools root directory" FORCE)

#Connecting the "dependencies" directory with the rest of the project

SET_PROPERTY(GLOBAL PROPERTY EP_BASE "${OR_TOOLS_ROOT_DIR}/dependencies")
GET_PROPERTY(V_EP_BASE GLOBAL PROPERTY EP_BASE)

set(CMAKE_INSTALL_PREFIX ${V_EP_BASE} CACHE INTERNAL "" FORCE)

set(DEPENDENCIES_SOURCES ${V_EP_BASE}/Source CACHE INTERNAL "" FORCE)

set(DEPENDENCIES_INSTALL ${V_EP_BASE}/Install CACHE INTERNAL "" FORCE)

set(DEPENDENCIES_ARCHIVES ${V_EP_BASE}/Archives CACHE INTERNAL "" FORCE)

set(DEPENDENCIES_INCLUDE ${V_EP_BASE}/include CACHE INTERNAL "" FORCE)

set(DEPENDENCIES_LIB ${V_EP_BASE}/lib CACHE INTERNAL "" FORCE)

set(MAKEFILE_LOCAL "${OR_TOOLS_ROOT_DIR}/Makefile.local")

#Using dependencies/Install/bin && dependencies/Install/lib
set(CMAKE_PREFIX_PATH "${CMAKE_INSTALL_PREFIX}" CACHE INTERNAL "" FORCE)

#########>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<###########

#[[
Define the or-tools directories.
]]

set(REL_SRC_DIR "src" CACHE INTERNAL "Relative or-tools sources directory" FORCE)
set(SRC_DIR "${OR_TOOLS_ROOT_DIR}/${REL_SRC_DIR}" CACHE INTERNAL "or-tools sources directory" FORCE)

set(REL_GEN_DIR "src/gen" CACHE INTERNAL "Relative or-tools generated sources directory" FORCE)
set(GEN_DIR "${OR_TOOLS_ROOT_DIR}/${REL_GEN_DIR}" CACHE INTERNAL "or-tools generated sources directory" FORCE)
file(MAKE_DIRECTORY ${GEN_DIR})

set(REL_EX_DIR "examples" CACHE INTERNAL "Relative or-tools examples directory" FORCE)
set(EX_DIR "${OR_TOOLS_ROOT_DIR}/${REL_EX_DIR}" CACHE INTERNAL "or-tools examples directory" FORCE)

set(REL_CPP_EX_DIR "examples/cpp" CACHE INTERNAL "Relative or-tools C++ examples directory" FORCE)
set(CPP_EX_DIR "${OR_TOOLS_ROOT_DIR}/${REL_CPP_EX_DIR}" CACHE INTERNAL "or-tools C++ examples directory" FORCE)

set(REL_CS_EX_DIR "examples/chsarp" CACHE INTERNAL "Relative or-tools C# examples directory" FORCE)
set(CS_EX_DIR "${OR_TOOLS_ROOT_DIR}/${REL_CS_EX_DIR}" CACHE INTERNAL "or-tools C# examples directory" FORCE)

set(REL_PYTHON_EX_DIR "examples/python" CACHE INTERNAL "Relative or-tools python examples directory" FORCE)
set(PYTHON_EX_DIR "${OR_TOOLS_ROOT_DIR}/${REL_PYTHON_EX_DIR}" CACHE INTERNAL "or-tools python examples directory" FORCE)

set(REL_FZ_EX_DIR "examples/flatzinc" CACHE INTERNAL "Relative or-tools flatzinc examples directory" FORCE)
set(FZ_EX_DIR "${OR_TOOLS_ROOT_DIR}/${REL_FZ_EX_DIR}" CACHE INTERNAL "or-tools flatzinc examples directory" FORCE)

set(REL_LIB_DIR "lib" CACHE INTERNAL "Relative or-tools lib directory" FORCE)
set(LIB_DIR "${OR_TOOLS_ROOT_DIR}/${REL_LIB_DIR}" CACHE INTERNAL "or-tools lib directory" FORCE)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${LIB_DIR}" CACHE INTERNAL "or-tools lib directory" FORCE)

set (REL_BIN_DIR "bin" CACHE INTERNAL "Relative or-tools bin directory" FORCE)
set (BIN_DIR "${OR_TOOLS_ROOT_DIR}/${REL_BIN_DIR}" CACHE INTERNAL "or-tools bin directory" FORCE)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}" CACHE INTERNAL "or-tools bin directory" FORCE)

set(SWIG_WRAP_DIR "${GEN_DIR}/swig_wrappers" CACHE INTERNAL "or-tools generated swig wrappers directory" FORCE)
file(MAKE_DIRECTORY ${SWIG_WRAP_DIR})

set(CLASS_DIR "${OR_TOOLS_ROOT_DIR}/class" CACHE INTERNAL "or-tools generated class files directory" FORCE)

set(PROTO_PYTHON_DIR ${DEPENDENCIES_SOURCES}/${PROTOBUF_DIR}/python CACHE INTERNAL "The python sources directory from protobuf" FORCE)


