cmake_minimum_required (VERSION 3.0.2 FATAL_ERROR)
project (OR_TOOLS)

include(utils.cmake)

#Setting the project directories
include(global_variables.cmake)

#Setting the compilers and the flags
include(compiler.cmake)

#Downloading and compiling the third_party softwares
add_subdirectory(dependencies)

#Compiling the C++ source code
add_subdirectory(src/cpp)

#Compiling the C++ examples
add_subdirectory(examples/cpp)

#Compiling the Flatinc source code
add_subdirectory(src/flatzinc)

#Compiling the Flatzinc examples
add_subdirectory(examples/flatzinc)

#Compiling the java sources
add_subdirectory(src/java)

#Compiling the java examples
add_subdirectory(examples/java)

#Compiling the python sources
add_subdirectory(src/python)

#Compiling the python examples
add_subdirectory(examples/python)
