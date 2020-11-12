if(NOT BUILD_SAMPLES AND NOT BUILD_GLOP)
  return()
endif()


if(BUILD_GLOP)
  if(APPLE)
    set(CMAKE_INSTALL_RPATH
      "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN")
  endif()

  add_executable(simple_glop_program simple_glop_program.cc)
  target_include_directories(simple_glop_program PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(simple_glop_program PRIVATE cxx_std_17)
  target_link_libraries(simple_glop_program PRIVATE glop)

  install(TARGETS simple_glop_program)

  if(BUILD_TESTING)
    add_test(NAME cxx_simple_glop_program COMMAND simple_glop_program)
  endif()
endif()

if(BUILD_CXX_SAMPLES)
  file(GLOB CXX_SRCS "*.cc")
  foreach(SAMPLE IN LISTS CXX_SRCS)
    add_cxx_sample(${SAMPLE})
  endforeach()
endif()
