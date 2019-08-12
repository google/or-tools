if (NOT BUILD_CXX)
	return()
endif()

project(ortools_examples)

if (APPLE)
	set(CMAKE_INSTALL_RPATH
		"@loader_path/../..;@loader_path/../lib;@loader_path")
else()
	set(CMAKE_INSTALL_RPATH "$ORIGIN/../../:$ORIGIN/../lib:$ORIGIN/")
endif()

get_filename_component(PARENT_SOURCE_DIR ${PROJECT_SOURCE_DIR} DIRECTORY)

include(GNUInstallDirs)
