cc_binary(
    name = "costas_array",
    srcs = ["costas_array.cc"],
    deps = [
        "//ortools/base",
        "//ortools/base:random",
        "//ortools/constraint_solver:cp",
    ],
)

cc_binary(
    name = "cryptarithm",
    srcs = ["cryptarithm.cc"],
    deps = [
        "@com_google_protobuf_cc//:protobuf",
        "//ortools/base",
        "//ortools/constraint_solver:cp",
    ],
)

cc_library(
    name = "cvrptw_lib",
    srcs = ["cvrptw_lib.cc"],
    hdrs = ["cvrptw_lib.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:random",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "cvrptw",
    srcs = ["cvrptw.cc"],
    deps = [
        ":cvrptw_lib",
        "//ortools/base",
        "//ortools/base:random",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "cvrp_disjoint_tw",
    srcs = ["cvrp_disjoint_tw.cc"],
    deps = [
        ":cvrptw_lib",
        "//ortools/base",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "cvrptw_with_breaks",
    srcs = ["cvrptw_with_breaks.cc"],
    deps = [
        ":cvrptw_lib",
        "//ortools/base",
        "//ortools/base:random",
        "//ortools/base:strings",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_enums_cc_proto",
        "//ortools/constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "cvrptw_with_resources",
    srcs = ["cvrptw_with_resources.cc"],
    deps = [
        ":cvrptw_lib",
        "//ortools/base",
        "//ortools/base:random",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "cvrptw_with_stop_times_and_resources",
    srcs = ["cvrptw_with_stop_times_and_resources.cc"],
    deps = [
        ":cvrptw_lib",
        "//ortools/base",
        "//ortools/base:random",
        "//ortools/base:strings",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "cvrptw_with_refueling",
    srcs = ["cvrptw_with_refueling.cc"],
    deps = [
        ":cvrptw_lib",
        "//ortools/base",
        "//ortools/base:random",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_flags",
    ],
)

cc_library(
    name = "print_dimacs_assignment",
    hdrs = ["print_dimacs_assignment.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:status",
        "//ortools/graph:ebert_graph",
        "//ortools/graph:linear_assignment",
    ],
)

cc_library(
    name = "parse_dimacs_assignment",
    srcs = ["parse_dimacs_assignment.cc"],
    hdrs = ["parse_dimacs_assignment.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:callback",
        "//ortools/graph:ebert_graph",
        "//ortools/graph:linear_assignment",
        "//ortools/util:filelineiter",
    ],
)

cc_binary(
    name = "dimacs_assignment",
    srcs = ["dimacs_assignment.cc"],
    deps = [
        ":parse_dimacs_assignment",
        ":print_dimacs_assignment",
        "//ortools/algorithms:hungarian",
        "//ortools/base",
        "//ortools/base:timer",
        "//ortools/graph:ebert_graph",
        "//ortools/graph:linear_assignment",
    ],
)

cc_binary(
    name = "dobble_ls",
    srcs = ["dobble_ls.cc"],
    deps = [
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/base:random",
        "//ortools/constraint_solver:cp",
        "//ortools/util:bitset",
    ],
)

cc_binary(
    name = "flow_api",
    srcs = ["flow_api.cc"],
    deps = [
        "//ortools/base",
        "//ortools/graph:ebert_graph",
        "//ortools/graph:max_flow",
        "//ortools/graph:min_cost_flow",
    ],
)

cc_library(
    name = "fap_parser",
    srcs = ["fap_parser.cc"],
    hdrs = ["fap_parser.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "fap_model_printer",
    srcs = ["fap_model_printer.cc"],
    hdrs = ["fap_model_printer.h"],
    deps = [
        ":fap_parser",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "fap_utilities",
    srcs = ["fap_utilities.cc"],
    hdrs = ["fap_utilities.h"],
    deps = [
        ":fap_parser",
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/constraint_solver:cp",
    ],
)

cc_binary(
    name = "frequency_assignment_problem",
    srcs = ["frequency_assignment_problem.cc"],
    deps = [
        ":fap_model_printer",
        ":fap_parser",
        ":fap_utilities",
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/constraint_solver:cp",
    ],
)

cc_binary(
    name = "golomb",
    srcs = ["golomb.cc"],
    deps = [
        "//ortools/base",
        "//ortools/constraint_solver:cp",
    ],
)

cc_binary(
    name = "integer_programming",
    srcs = ["integer_programming.cc"],
    deps = [
        "//ortools/base",
        "//ortools/linear_solver:linear_solver",
    ],
)

cc_library(
    name = "jobshop_reader",
    hdrs = ["jobshop.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:strings",
        "//ortools/util:filelineiter",
    ],
)

cc_library(
    name = "flexible_jobshop_reader",
    hdrs = ["flexible_jobshop.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:strings",
        "//ortools/util:filelineiter",
        "//ortools/util:string_array",
    ],
)

cc_binary(
    name = "flexible_jobshop",
    srcs = [
        "flexible_jobshop.cc",
    ],
    deps = [
        ":flexible_jobshop_reader",
        "//ortools/base",
        "//ortools/base:strings",
        "//ortools/constraint_solver:cp",
    ],
)

cc_binary(
    name = "jobshop",
    srcs = [
        "jobshop.cc",
    ],
    deps = [
        ":jobshop_reader",
        "//ortools/base",
        "//ortools/base:strings",
        "//ortools/constraint_solver:cp",
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
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:strings",
        "//ortools/base:timer",
        "//ortools/sat:disjunctive",
        "//ortools/sat:integer",
        "//ortools/sat:intervals",
        "//ortools/sat:model",
        "//ortools/sat:optimization",
        "//ortools/sat:precedences",
        "//ortools/sat:sat_solver",
    ],
)

cc_binary(
    name = "linear_programming",
    srcs = ["linear_programming.cc"],
    copts = ["-DUSE_GLOP"],
    deps = [
        "//ortools/base",
        "//ortools/linear_solver:linear_solver",
        "//ortools/linear_solver:linear_solver_cc_proto",
    ],
)

cc_binary(
    name = "linear_solver_protocol_buffers",
    srcs = ["linear_solver_protocol_buffers.cc"],
    deps = [
        "//ortools/base",
        "//ortools/linear_solver:linear_solver",
        "//ortools/linear_solver:linear_solver_cc_proto",
    ],
)

cc_binary(
    name = "ls_api",
    srcs = ["ls_api.cc"],
    deps = [
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/base:random",
        "//ortools/base:stl_util",
        "//ortools/constraint_solver:cp",
    ],
)

cc_binary(
    name = "magic_square",
    srcs = ["magic_square.cc"],
    deps = [
        "//ortools/base",
        "//ortools/constraint_solver:cp",
    ],
)

cc_binary(
    name = "network_routing",
    srcs = ["network_routing.cc"],
    deps = [
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/base:random",
        "//ortools/constraint_solver:cp",
        "//ortools/graph:shortestpaths",
        "//ortools/util:tuple_set",
    ],
)

cc_binary(
    name = "rcpsp_sat",
    srcs = [
        "rcpsp_sat.cc",
    ],
    deps = [
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:strings",
        "//ortools/sat:cp_constraints",
        "//ortools/sat:cumulative",
        "//ortools/sat:disjunctive",
        "//ortools/sat:integer",
        "//ortools/sat:integer_expr",
        "//ortools/sat:intervals",
        "//ortools/sat:model",
        "//ortools/sat:optimization",
        "//ortools/sat:precedences",
        "//ortools/sat:sat_solver",
        "//ortools/util:filelineiter",
        "//ortools/util:rcpsp_parser",
    ],
)

cc_binary(
    name = "nqueens",
    srcs = ["nqueens.cc"],
    deps = [
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/constraint_solver:cp",
    ],
)

cc_binary(
    name = "pdptw",
    srcs = ["pdptw.cc"],
    deps = [
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:mathutil",
        "//ortools/base:strings",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_flags",
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
        "//ortools/sat:boolean_problem",
        "//ortools/sat:boolean_problem_cc_proto",
        #        "//ortools/sat:cp_model_proto",
        #        "//ortools/sat:cp_model_solver",
        "//ortools/sat:drat",
        "//ortools/sat:lp_utils",
        "//ortools/sat:optimization",
        "//ortools/sat:sat_solver",
        "//ortools/sat:simplification",
        "//ortools/sat:symmetry",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:strings",
        "//ortools/base:status",
        "//ortools/base:random",
        "//ortools/base:threadpool",
        "//ortools/algorithms:sparse_permutation",
        "//ortools/lp_data:mps_reader",
        "//ortools/lp_data:proto_utils",
        "//ortools/util:filelineiter",
        "//ortools/util:time_limit",
    ],
)

#cc_binary(
#    name = "shift_minimization_sat",
#    srcs = [
#        "shift_minimization_sat.cc",
#    ],
#    deps = [
#        "//ortools/base",
#        "//ortools/base:file",
#        "//ortools/base:strings",
#        "//ortools/sat:cp_constraints",
#        "//ortools/sat:integer",
#        "//ortools/sat:integer_expr",
#        "//ortools/sat:model",
#        "//ortools/sat:optimization",
#        "//ortools/sat:precedences",
#        "//ortools/sat:sat_solver",
#        "//ortools/util:bp_parser",
#        "//ortools/util:filelineiter",
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
        "//ortools/base",
        "//ortools/linear_solver:linear_solver",
        "//ortools/linear_solver:linear_solver_cc_proto",
        "//ortools/lp_data:mps_reader",
    ],
)

cc_binary(
    name = "sports_scheduling",
    srcs = ["sports_scheduling.cc"],
    deps = [
        "//ortools/base",
        "//ortools/constraint_solver:cp",
    ],
)

cc_binary(
    name = "strawberry_fields_with_column_generation",
    srcs = ["strawberry_fields_with_column_generation.cc"],
    copts = ["-DUSE_GLOP"],
    deps = [
        "//ortools/base",
        "//ortools/linear_solver:linear_solver",
    ],
)

cc_binary(
    name = "tsp",
    srcs = ["tsp.cc"],
    deps = [
        "@com_google_protobuf_cc//:protobuf",
        "//ortools/base",
        "//ortools/constraint_solver:cp",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "weighted_tardiness_sat",
    srcs = [
        "weighted_tardiness_sat.cc",
    ],
    deps = [
        "@com_google_protobuf_cc//:protobuf",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:strings",
        "//ortools/sat:disjunctive",
        "//ortools/sat:integer",
        "//ortools/sat:integer_expr",
        "//ortools/sat:intervals",
        "//ortools/sat:model",
        "//ortools/sat:optimization",
        "//ortools/sat:precedences",
        "//ortools/sat:sat_solver",
        "//ortools/util:filelineiter",
    ],
)
