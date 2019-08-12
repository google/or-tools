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


if (USE_SIRIUS)
	add_test(NAME "cc_strawberry_fields_with_column_generation_glop" COMMAND strawberry_fields_with_column_generation -colgen_solver glop)
	add_test(NAME "cc_strawberry_fields_with_column_generation_clp" COMMAND strawberry_fields_with_column_generation -colgen_solver clp)
	add_test(NAME "cc_strawberry_fields_with_column_generation_sirius" COMMAND strawberry_fields_with_column_generation -colgen_solver sirius)
endif()

if (USE_CPLEX)
	add_test(NAME "cc_strawberry_fields_with_column_generation_cplex" COMMAND strawberry_fields_with_column_generation -colgen_solver cplex)
endif()