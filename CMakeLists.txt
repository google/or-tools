cmake_minimum_required (VERSION 3.0.2 FATAL_ERROR)
project (OR_TOOLS)

#Setting the project variables
add_subdirectory(cmake)

#Downloading and compiling the third_party softwares
add_subdirectory(dependencies/cmake)

#Compiling the or-tools core source code
add_subdirectory(${REL_SRC_DIR}/cmake)

#Compiling the C++ examples
add_subdirectory(${REL_EXAMPLES_DIR}/cpp/cmake)
