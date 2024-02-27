find_package(sirius_solver REQUIRED)

add_library(sirius_interface
	    sirius_interface.h
	    sirius_interface.cc)

target_link_libraries(sirius_interface
			PRIVATE
			sirius_solver
			ortools::ortools)

add_executable(client_sirius client.cc)

target_link_libraries(client_sirius
			PRIVATE
			sirius_interface
			ortools::ortools)
