cmake_minimum_required(VERSION 3.5)

project(swig-download NONE)

include(ExternalProject)
ExternalProject_Add(SWIG_project
	URL "http://prdownloads.sourceforge.net/swig/swigwin-3.0.12.zip"
	BUILD_IN_SOURCE 1
	SOURCE_DIR "${CMAKE_BINARY_DIR}/swig"
	UPDATE_COMMAND ""
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
	TEST_COMMAND ""
	LOG_DOWNLOAD ON
	LOG_CONFIGURE ON
	LOG_BUILD ON
)