if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  return()
endif()

# WASM CP-SAT API target (kept separate so other targets remain unchanged).
add_executable(cp_sat_api cp_sat_api.cc)
target_link_libraries(cp_sat_api PRIVATE ${PROJECT_NAMESPACE}::ortools)

set_target_properties(cp_sat_api PROPERTIES
  OUTPUT_NAME "cp_sat_api"
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/javascript")

# Emscripten link options for a modular JS/WASM bundle exporting the C API.
target_link_options(cp_sat_api PRIVATE
  "-sMODULARIZE=1"
  "-sEXPORT_NAME=cpSatModule"
  "-sENVIRONMENT=web,worker,node"
  "-sUSE_PTHREADS=1"
  "-sPTHREAD_POOL_SIZE=32"
  "-sASYNCIFY=1"
  "-sEXPORTED_FUNCTIONS=['_malloc','_free','_solve_model','_free_buffer','_solve_magic_square']"
  "-sEXPORTED_RUNTIME_METHODS=callMain,ccall,cwrap,UTF8ToString,stringToUTF8,allocateUTF8,HEAPU8"
  "-pthread"
  "--shared-memory"
  "--no-entry"
  "-gsource-map"
  "--source-map-base=./")

# Simple static test page next to the generated JS/WASM.
configure_file(site/index.html "${PROJECT_BINARY_DIR}/javascript/index.html" COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/ortools/sat/cp_model.proto
  "${PROJECT_BINARY_DIR}/javascript/cp_model.proto" COPYONLY)
