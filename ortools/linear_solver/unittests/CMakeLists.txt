if(NOT BUILD_LINEAR_SOLVER_UNITTESTS)
	return()
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        703bd9caab50b139428cea1aaff9974ebee5742e # release-1.10.0
)
FetchContent_MakeAvailable(googletest)


if(BUILD_CXX_EXAMPLES)
  if(APPLE)
    set(CMAKE_INSTALL_RPATH
            "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN")
  endif()
  if(BUILD_TESTING)
    if (USE_SIRIUS)
      add_executable(test_sirius_interface sirius_interface.cc)
      target_include_directories(test_sirius_interface PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
      target_compile_features(test_sirius_interface PRIVATE cxx_std_17)
      target_link_libraries(test_sirius_interface PRIVATE ortools::ortools gtest)

      add_test(NAME cxx_unittests_sirius_interface COMMAND test_sirius_interface)
    endif()
    add_executable(xprs_interface xpress_interface.cc)
    target_include_directories(xprs_interface PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(xprs_interface PRIVATE cxx_std_17)
    target_link_libraries(xprs_interface PRIVATE ortools::ortools gtest)

    add_test(NAME cxx_unittests_xpress_interface COMMAND xprs_interface)
  endif()
endif()
