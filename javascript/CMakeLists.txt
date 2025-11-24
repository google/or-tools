if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  return()
endif()

# Embed proto schemas directly into the WASM binary so browsers don't need to
# fetch .proto files separately.
set(PROTO_SCHEMA_HEADER "${PROJECT_BINARY_DIR}/javascript/generated_proto_schemas.h")
add_custom_command(
  OUTPUT "${PROTO_SCHEMA_HEADER}"
  COMMAND "${CMAKE_COMMAND}"
          -DCP_MODEL_PROTO="${PROJECT_SOURCE_DIR}/ortools/sat/cp_model.proto"
          -DSAT_PARAMETERS_PROTO="${PROJECT_SOURCE_DIR}/ortools/sat/sat_parameters.proto"
          -DTEMPLATE_FILE="${PROJECT_SOURCE_DIR}/javascript/generated_proto_schemas.h.in"
          -DOUTPUT_FILE="${PROTO_SCHEMA_HEADER}"
          -P "${PROJECT_SOURCE_DIR}/scripts/embed_proto.cmake"
  DEPENDS
    "${PROJECT_SOURCE_DIR}/scripts/embed_proto.cmake"
    "${PROJECT_SOURCE_DIR}/javascript/generated_proto_schemas.h.in"
    "${PROJECT_SOURCE_DIR}/ortools/sat/cp_model.proto"
    "${PROJECT_SOURCE_DIR}/ortools/sat/sat_parameters.proto")

# WASM CP-SAT API target (kept separate so other targets remain unchanged).
add_executable(cp_sat_api cp_sat_api.cc "${PROTO_SCHEMA_HEADER}")
target_link_libraries(cp_sat_api PRIVATE ${PROJECT_NAMESPACE}::ortools)
target_include_directories(cp_sat_api PRIVATE "${PROJECT_BINARY_DIR}/javascript")

set_target_properties(cp_sat_api PROPERTIES
  OUTPUT_NAME "cp_sat_api"
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/javascript")

set(JS_OUTPUT_DIR "${PROJECT_BINARY_DIR}/javascript")
set(WEB_DEPLOY_DIR "${PROJECT_BINARY_DIR}/bin")
set(JS_EXAMPLES_DIR "${JS_OUTPUT_DIR}/examples")
set(WEB_EXAMPLES_DIR "${WEB_DEPLOY_DIR}/examples")
file(MAKE_DIRECTORY "${JS_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${WEB_DEPLOY_DIR}")
file(MAKE_DIRECTORY "${JS_EXAMPLES_DIR}")
file(MAKE_DIRECTORY "${WEB_EXAMPLES_DIR}")

# Emscripten link options for a modular JS/WASM bundle exporting the C API.
target_link_options(cp_sat_api PRIVATE
  "-sMODULARIZE=1"
  "-sEXPORT_NAME=cpSatModule"
  "-sENVIRONMENT=web,worker,node"
  "-sUSE_PTHREADS=1"
  "-sPTHREAD_POOL_SIZE=32"
  "-sASYNCIFY=1"
  "--bind"
  "--emit-tsd"
  "${JS_OUTPUT_DIR}/cp_sat_api.d.ts"
  "-sEXPORTED_FUNCTIONS=['_malloc','_free','_solve_model','_free_buffer','_validate_model','_get_cp_model_schema','_get_sat_parameters_schema']"
  "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,allocateUTF8,HEAPU8"
  "-pthread"
  "--shared-memory"
  "--no-entry"
  "-gsource-map"
  "--source-map-base=./")

# Static site assets copied to both javascript/ and bin/ for serving.
set(JS_SITE_ROOT_FILES
  "${PROJECT_SOURCE_DIR}/javascript/site/cpsat_api.js"
  "${PROJECT_SOURCE_DIR}/javascript/site/cpsat_api.js.map"
  "${PROJECT_SOURCE_DIR}/javascript/site/index.html")
foreach(site_file IN LISTS JS_SITE_ROOT_FILES)
  get_filename_component(site_name "${site_file}" NAME)
  foreach(dst "${JS_OUTPUT_DIR}" "${WEB_DEPLOY_DIR}")
    configure_file("${site_file}" "${dst}/${site_name}" COPYONLY)
  endforeach()
endforeach()

set(JS_SITE_EXAMPLE_FILES
  "${PROJECT_SOURCE_DIR}/javascript/site/magic_square.html"
  "${PROJECT_SOURCE_DIR}/javascript/site/magic_square.js"
  "${PROJECT_SOURCE_DIR}/javascript/site/magic_square.js.map"
  "${PROJECT_SOURCE_DIR}/javascript/site/schema_viewer.html"
  "${PROJECT_SOURCE_DIR}/javascript/site/schema_viewer.js"
  "${PROJECT_SOURCE_DIR}/javascript/site/schema_viewer.js.map")
foreach(site_file IN LISTS JS_SITE_EXAMPLE_FILES)
  get_filename_component(site_name "${site_file}" NAME)
  foreach(dst "${JS_EXAMPLES_DIR}" "${WEB_EXAMPLES_DIR}")
    configure_file("${site_file}" "${dst}/${site_name}" COPYONLY)
  endforeach()
endforeach()

add_custom_command(TARGET cp_sat_api POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${WEB_DEPLOY_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${JS_OUTPUT_DIR}" "${WEB_DEPLOY_DIR}"
  COMMENT "Copying cp_sat_api artifacts into ${WEB_DEPLOY_DIR}")
