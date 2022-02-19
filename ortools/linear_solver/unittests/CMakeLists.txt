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

function(add_cxx_unittest FILE_NAME)
  message(STATUS "Configuring unittest ${FILE_NAME}: ...")
  get_filename_component(TEST_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(COMPONENT_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  if(APPLE)
    set(CMAKE_INSTALL_RPATH
      "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN")
  endif()

  add_executable(${TEST_NAME} ${FILE_NAME})
  target_include_directories(${TEST_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(${TEST_NAME} PRIVATE cxx_std_17)
  target_link_libraries(${TEST_NAME} PRIVATE ortools::ortools gtest)

  if(BUILD_TESTING)
    add_test(NAME cxx_${COMPONENT_NAME}_${TEST_NAME} COMMAND ${TEST_NAME})
  endif()
  message(STATUS "Configuring unittest ${FILE_NAME}: ...DONE")
endfunction()

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

      add_test(NAME cxx_test_sirius_interface COMMAND test_sirius_interface)
    endif()
    if  (USE_XPRESS)
      add_executable(xprs_interface xpress_interface.cc)
      target_include_directories(xprs_interface PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
      target_compile_features(xprs_interface PRIVATE cxx_std_17)
      target_link_libraries(xprs_interface PRIVATE ortools::ortools gtest)

      add_test(NAME cxx_test_xpress_interface COMMAND xprs_interface)
    endif()
  endif()
  #file(GLOB CXX_SRCS "*.cc")
  #foreach(FILE_NAME IN LISTS CXX_SRCS)
  #  add_cxx_unittest(${FILE_NAME})
  #endforeach()
endif()
