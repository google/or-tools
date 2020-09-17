if(NOT BUILD_SAMPLES)
  return()
endif()

if(BUILD_CXX_SAMPLES)
  if(APPLE)
    set(CMAKE_INSTALL_RPATH
        "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN")
  endif()

  include(GNUInstallDirs)
  foreach(EXECUTABLE IN ITEMS simple_max_flow_program
                              simple_min_cost_flow_program)
    add_executable(${EXECUTABLE} ${EXECUTABLE}.cc)
    target_include_directories(${EXECUTABLE} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(${EXECUTABLE} PRIVATE cxx_std_17)
    target_link_libraries(${EXECUTABLE} PRIVATE ortools::ortools)
    install(TARGETS ${EXECUTABLE})
    add_test(NAME cxx_${EXECUTABLE} COMMAND ${EXECUTABLE})
  endforeach()
endif()

if(BUILD_PYTHON_SAMPLES)
  file(GLOB PYTHON_SRCS "*.py")
  foreach(SAMPLE IN LISTS PYTHON_SRCS)
    add_python_sample(${SAMPLE})
  endforeach()
endif()

if(BUILD_JAVA_SAMPLES)
  file(GLOB JAVA_SRCS "*.java")
  foreach(SAMPLE IN LISTS JAVA_SRCS)
    add_java_sample(${SAMPLE})
  endforeach()
endif()

if(BUILD_DOTNET_SAMPLES)

endif()
