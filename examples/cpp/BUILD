cc_binary(
    name = "costas_array",
    srcs = ["costas_array.cc"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:random",
        "@or_tools_libraries//constraint_solver:cp",
    ],
)

cc_binary(
    name = "cryptarithm",
    srcs = ["cryptarithm.cc"],
    deps = [
        "@com_google_protobuf_cc//:protobuf",
        "@or_tools_libraries//base",
        "@or_tools_libraries//constraint_solver:cp",
    ],
)

cc_library(
    name = "cvrptw_lib",
    srcs = ["cvrptw_lib.cc"],
    hdrs = ["cvrptw_lib.h"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:random",
        "@or_tools_libraries//constraint_solver:routing",
        "@or_tools_libraries//constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "cvrptw",
    srcs = ["cvrptw.cc"],
    deps = [
        ":cvrptw_lib",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:random",
        "@or_tools_libraries//constraint_solver:routing",
        "@or_tools_libraries//constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "cvrp_disjoint_tw",
    srcs = ["cvrp_disjoint_tw.cc"],
    deps = [
        ":cvrptw_lib",
        "@or_tools_libraries//base",
        "@or_tools_libraries//constraint_solver:routing",
        "@or_tools_libraries//constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "cvrptw_with_breaks",
    srcs = ["cvrptw_with_breaks.cc"],
    deps = [
        ":cvrptw_lib",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:random",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//constraint_solver:routing",
        "@or_tools_libraries//constraint_solver:routing_enums_cc_proto",
        "@or_tools_libraries//constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "cvrptw_with_resources",
    srcs = ["cvrptw_with_resources.cc"],
    deps = [
        ":cvrptw_lib",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:random",
        "@or_tools_libraries//constraint_solver:routing",
        "@or_tools_libraries//constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "cvrptw_with_stop_times_and_resources",
    srcs = ["cvrptw_with_stop_times_and_resources.cc"],
    deps = [
        ":cvrptw_lib",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:random",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//constraint_solver:routing",
        "@or_tools_libraries//constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "cvrptw_with_refueling",
    srcs = ["cvrptw_with_refueling.cc"],
    deps = [
        ":cvrptw_lib",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:random",
        "@or_tools_libraries//constraint_solver:routing",
        "@or_tools_libraries//constraint_solver:routing_flags",
    ],
)

cc_library(
    name = "print_dimacs_assignment",
    hdrs = ["print_dimacs_assignment.h"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:file",
        "@or_tools_libraries//base:status",
        "@or_tools_libraries//graph:ebert_graph",
        "@or_tools_libraries//graph:linear_assignment",
    ],
)

cc_library(
    name = "parse_dimacs_assignment",
    srcs = ["parse_dimacs_assignment.cc"],
    hdrs = ["parse_dimacs_assignment.h"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:callback",
        "@or_tools_libraries//graph:ebert_graph",
        "@or_tools_libraries//graph:linear_assignment",
        "@or_tools_libraries//util:filelineiter",
    ],
)

cc_binary(
    name = "dimacs_assignment",
    srcs = ["dimacs_assignment.cc"],
    deps = [
        ":parse_dimacs_assignment",
        ":print_dimacs_assignment",
        "@or_tools_libraries//algorithms:hungarian",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:timer",
        "@or_tools_libraries//graph:ebert_graph",
        "@or_tools_libraries//graph:linear_assignment",
    ],
)

cc_binary(
    name = "dobble_ls",
    srcs = ["dobble_ls.cc"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:map_util",
        "@or_tools_libraries//base:random",
        "@or_tools_libraries//constraint_solver:cp",
        "@or_tools_libraries//util:bitset",
    ],
)

cc_binary(
    name = "flow_api",
    srcs = ["flow_api.cc"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//graph:ebert_graph",
        "@or_tools_libraries//graph:max_flow",
        "@or_tools_libraries//graph:min_cost_flow",
    ],
)

cc_library(
    name = "fap_parser",
    srcs = ["fap_parser.cc"],
    hdrs = ["fap_parser.h"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:file",
        "@or_tools_libraries//base:hash",
        "@or_tools_libraries//base:map_util",
        "@or_tools_libraries//base:strings",
    ],
)

cc_library(
    name = "fap_model_printer",
    srcs = ["fap_model_printer.cc"],
    hdrs = ["fap_model_printer.h"],
    deps = [
        ":fap_parser",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:file",
        "@or_tools_libraries//base:strings",
    ],
)

cc_library(
    name = "fap_utilities",
    srcs = ["fap_utilities.cc"],
    hdrs = ["fap_utilities.h"],
    deps = [
        ":fap_parser",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:map_util",
        "@or_tools_libraries//constraint_solver:cp",
    ],
)

cc_binary(
    name = "frequency_assignment_problem",
    srcs = ["frequency_assignment_problem.cc"],
    deps = [
        ":fap_model_printer",
        ":fap_parser",
        ":fap_utilities",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:map_util",
        "@or_tools_libraries//constraint_solver:cp",
    ],
)

cc_binary(
    name = "golomb",
    srcs = ["golomb.cc"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//constraint_solver:cp",
    ],
)

cc_binary(
    name = "integer_programming",
    srcs = ["integer_programming.cc"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//linear_solver:linear_solver",
    ],
)

cc_library(
    name = "jobshop_reader",
    hdrs = ["jobshop.h"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//util:filelineiter",
    ],
)

cc_library(
    name = "flexible_jobshop_reader",
    hdrs = ["flexible_jobshop.h"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//util:filelineiter",
        "@or_tools_libraries//util:string_array",
    ],
)

cc_binary(
    name = "flexible_jobshop",
    srcs = [
        "flexible_jobshop.cc",
    ],
    deps = [
        ":flexible_jobshop_reader",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//constraint_solver:cp",
    ],
)

cc_binary(
    name = "jobshop",
    srcs = [
        "jobshop.cc",
    ],
    deps = [
        ":jobshop_reader",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//constraint_solver:cp",
    ],
)

cc_binary(
    name = "jobshop_sat",
    srcs = [
        "jobshop_sat.cc",
    ],
    deps = [
        ":flexible_jobshop_reader",
        ":jobshop_reader",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:file",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//base:timer",
        "@or_tools_libraries//sat:disjunctive",
        "@or_tools_libraries//sat:integer",
        "@or_tools_libraries//sat:intervals",
        "@or_tools_libraries//sat:model",
        "@or_tools_libraries//sat:optimization",
        "@or_tools_libraries//sat:precedences",
        "@or_tools_libraries//sat:sat_solver",
    ],
)

cc_binary(
    name = "linear_programming",
    srcs = ["linear_programming.cc"],
    copts = ["-DUSE_GLOP"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//linear_solver:linear_solver",
        "@or_tools_libraries//linear_solver:linear_solver_cc_proto",
    ],
)

cc_binary(
    name = "linear_solver_protocol_buffers",
    srcs = ["linear_solver_protocol_buffers.cc"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//linear_solver:linear_solver",
        "@or_tools_libraries//linear_solver:linear_solver_cc_proto",
    ],
)

cc_binary(
    name = "ls_api",
    srcs = ["ls_api.cc"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:hash",
        "@or_tools_libraries//base:map_util",
        "@or_tools_libraries//base:random",
        "@or_tools_libraries//base:stl_util",
        "@or_tools_libraries//constraint_solver:cp",
    ],
)

cc_binary(
    name = "magic_square",
    srcs = ["magic_square.cc"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//constraint_solver:cp",
    ],
)

cc_binary(
    name = "network_routing",
    srcs = ["network_routing.cc"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:hash",
        "@or_tools_libraries//base:map_util",
        "@or_tools_libraries//base:random",
        "@or_tools_libraries//constraint_solver:cp",
        "@or_tools_libraries//graph:shortestpaths",
        "@or_tools_libraries//util:tuple_set",
    ],
)

cc_binary(
    name = "rcpsp_sat",
    srcs = [
        "rcpsp_sat.cc",
    ],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:file",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//sat:cp_constraints",
        "@or_tools_libraries//sat:cumulative",
        "@or_tools_libraries//sat:disjunctive",
        "@or_tools_libraries//sat:integer",
        "@or_tools_libraries//sat:integer_expr",
        "@or_tools_libraries//sat:intervals",
        "@or_tools_libraries//sat:model",
        "@or_tools_libraries//sat:optimization",
        "@or_tools_libraries//sat:precedences",
        "@or_tools_libraries//sat:sat_solver",
        "@or_tools_libraries//util:filelineiter",
        "@or_tools_libraries//util:rcpsp_parser",
    ],
)

cc_binary(
    name = "nqueens",
    srcs = ["nqueens.cc"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:map_util",
        "@or_tools_libraries//constraint_solver:cp",
    ],
)

cc_binary(
    name = "pdptw",
    srcs = ["pdptw.cc"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:file",
        "@or_tools_libraries//base:mathutil",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//constraint_solver:routing",
        "@or_tools_libraries//constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "sat_runner",
    srcs = [
        "opb_reader.h",
        "sat_cnf_reader.h",
        "sat_runner.cc",
    ],
    deps = [
        "@com_google_protobuf_cc//:protobuf",
        "@or_tools_libraries//sat:boolean_problem",
        "@or_tools_libraries//sat:boolean_problem_cc_proto",
        #        "@or_tools_libraries//sat:cp_model_proto",
        #        "@or_tools_libraries//sat:cp_model_solver",
        "@or_tools_libraries//sat:drat",
        "@or_tools_libraries//sat:lp_utils",
        "@or_tools_libraries//sat:optimization",
        "@or_tools_libraries//sat:sat_solver",
        "@or_tools_libraries//sat:simplification",
        "@or_tools_libraries//sat:symmetry",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:file",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//base:status",
        "@or_tools_libraries//base:random",
        "@or_tools_libraries//base:threadpool",
        "@or_tools_libraries//algorithms:sparse_permutation",
        "@or_tools_libraries//lp_data:mps_reader",
        "@or_tools_libraries//lp_data:proto_utils",
        "@or_tools_libraries//util:filelineiter",
        "@or_tools_libraries//util:time_limit",
    ],
)

#cc_binary(
#    name = "shift_minimization_sat",
#    srcs = [
#        "shift_minimization_sat.cc",
#    ],
#    deps = [
#        "@or_tools_libraries//base",
#        "@or_tools_libraries//base:file",
#        "@or_tools_libraries//base:strings",
#        "@or_tools_libraries//sat:cp_constraints",
#        "@or_tools_libraries//sat:integer",
#        "@or_tools_libraries//sat:integer_expr",
#        "@or_tools_libraries//sat:model",
#        "@or_tools_libraries//sat:optimization",
#        "@or_tools_libraries//sat:precedences",
#        "@or_tools_libraries//sat:sat_solver",
#        "@or_tools_libraries//util:bp_parser",
#        "@or_tools_libraries//util:filelineiter",
#    ],
#)

cc_binary(
    name = "solve",
    srcs = ["solve.cc"],
    copts = [
        "-DUSE_GLOP",
        "-DUSE_BOP",
    ],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//linear_solver:linear_solver",
        "@or_tools_libraries//linear_solver:linear_solver_cc_proto",
        "@or_tools_libraries//lp_data:mps_reader",
    ],
)

cc_binary(
    name = "sports_scheduling",
    srcs = ["sports_scheduling.cc"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//constraint_solver:cp",
    ],
)

cc_binary(
    name = "strawberry_fields_with_column_generation",
    srcs = ["strawberry_fields_with_column_generation.cc"],
    copts = ["-DUSE_GLOP"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//linear_solver:linear_solver",
    ],
)

cc_binary(
    name = "tsp",
    srcs = ["tsp.cc"],
    deps = [
        "@com_google_protobuf_cc//:protobuf",
        "@or_tools_libraries//base",
        "@or_tools_libraries//constraint_solver:cp",
        "@or_tools_libraries//constraint_solver:routing",
        "@or_tools_libraries//constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "weighted_tardiness_sat",
    srcs = [
        "weighted_tardiness_sat.cc",
    ],
    deps = [
        "@com_google_protobuf_cc//:protobuf",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:file",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//sat:disjunctive",
        "@or_tools_libraries//sat:integer",
        "@or_tools_libraries//sat:integer_expr",
        "@or_tools_libraries//sat:intervals",
        "@or_tools_libraries//sat:model",
        "@or_tools_libraries//sat:optimization",
        "@or_tools_libraries//sat:precedences",
        "@or_tools_libraries//sat:sat_solver",
        "@or_tools_libraries//util:filelineiter",
    ],
)
