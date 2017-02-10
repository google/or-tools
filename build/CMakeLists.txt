cmake_minimum_required (VERSION 3.0.2 FATAL_ERROR)
project (OR_TOOLS)

#Setting the project variables
include(global_variables.cmake)

#Downloading and compiling the third_party softwares
add_subdirectory(dependencies)

#Compiling the or-tools core source code
add_subdirectory(src/cpp)

#Compiling the C++ examples
add_subdirectory(examples/cpp)

#Compiling the or-tools core source code
add_subdirectory(src/flatzinc)

#Compiling the C++ examples
add_subdirectory(examples/flatzinc)
