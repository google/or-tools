if (NOT BUILD_XPRESS_TEST_AND_EXAMPLES)
    return()
endif ()

if (BUILD_CXX_EXAMPLES)
    add_cxx_example(${CMAKE_CURRENT_SOURCE_DIR}/xpress_use.cc)
endif ()

# disable python callback test
#if (BUILD_PYTHON_EXAMPLES)
#    add_python_example(${CMAKE_CURRENT_SOURCE_DIR}/callback_xpress.py)
#endif ()

include(CTest)
if (BUILD_CXX_EXAMPLES)
    if (APPLE)
        set(CMAKE_INSTALL_RPATH
                "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
    elseif (UNIX)
        set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN:$ORIGIN/../lib:$ORIGIN")
    endif ()

    add_executable(test_xprs_interface test/test_xpress_interface.cc)
    target_compile_features(test_xprs_interface PRIVATE cxx_std_17)
    target_link_libraries(test_xprs_interface PRIVATE ortools::ortools GTest::gtest_main)

    add_test(NAME cxx_unittests_xpress_interface COMMAND test_xprs_interface)
endif ()