if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  return()
endif()

# WASM CP-SAT API target (kept separate so other targets remain unchanged).
add_executable(cp_sat_api cp_sat_api.cc)
target_link_libraries(cp_sat_api PRIVATE ${PROJECT_NAMESPACE}::ortools)

set_target_properties(cp_sat_api PROPERTIES
  OUTPUT_NAME "cp_sat_api"
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/javascript")

set(JS_OUTPUT_DIR "${PROJECT_BINARY_DIR}/javascript")
set(WEB_DEPLOY_DIR "${PROJECT_BINARY_DIR}/bin")
file(MAKE_DIRECTORY "${JS_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${WEB_DEPLOY_DIR}")

# Emscripten link options for a modular JS/WASM bundle exporting the C API.
target_link_options(cp_sat_api PRIVATE
  "-sMODULARIZE=1"
  "-sEXPORT_NAME=cpSatModule"
  "-sENVIRONMENT=web,worker,node"
  "-sUSE_PTHREADS=1"
  "-sPTHREAD_POOL_SIZE=32"
  "-sASYNCIFY=1"
  "-sEXPORTED_FUNCTIONS=['_malloc','_free','_solve_model','_free_buffer','_solve_magic_square']"
  "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,allocateUTF8,HEAPU8"
  "-pthread"
  "--shared-memory"
  "--no-entry"
  "-gsource-map"
  "--source-map-base=./")

# Static site assets copied to both javascript/ and bin/ for serving.
set(JS_SITE_FILES
  "${PROJECT_SOURCE_DIR}/javascript/site/index.html"
  "${PROJECT_SOURCE_DIR}/javascript/site/magic_square.html")
foreach(site_file IN LISTS JS_SITE_FILES)
  get_filename_component(site_name "${site_file}" NAME)
  foreach(dst "${JS_OUTPUT_DIR}" "${WEB_DEPLOY_DIR}")
    configure_file("${site_file}" "${dst}/${site_name}" COPYONLY)
  endforeach()
endforeach()

set(JS_STATIC_FILES "${PROJECT_SOURCE_DIR}/ortools/sat/cp_model.proto")
foreach(static_file IN LISTS JS_STATIC_FILES)
  get_filename_component(static_name "${static_file}" NAME)
  foreach(dst "${JS_OUTPUT_DIR}" "${WEB_DEPLOY_DIR}")
    configure_file("${static_file}" "${dst}/${static_name}" COPYONLY)
  endforeach()
endforeach()

add_custom_command(TARGET cp_sat_api POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${WEB_DEPLOY_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${JS_OUTPUT_DIR}" "${WEB_DEPLOY_DIR}"
  COMMENT "Copying cp_sat_api artifacts into ${WEB_DEPLOY_DIR}")
