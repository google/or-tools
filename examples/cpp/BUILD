cc_binary(
    name = "constraint_programming_cp",
    srcs = ["constraint_programming_cp.cc"],
    deps = [
        "//ortools/base",
        "//ortools/constraint_solver:cp",
    ],
)

cc_binary(
    name = "costas_array_sat",
    srcs = ["costas_array_sat.cc"],
    deps = [
        "//ortools/base",
        "//ortools/base:random",
        "//ortools/sat:cp_model",
        "//ortools/sat:model",
    ],
)

cc_library(
    name = "cvrptw_lib",
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
        "@com_google_absl//absl/strings",
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
        "@com_google_absl//absl/strings",
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
        "@com_google_absl//absl/status",
        "//ortools/graph:ebert_graph",
        "//ortools/graph:linear_assignment",
    ],
)

cc_library(
    name = "parse_dimacs_assignment",
    hdrs = ["parse_dimacs_assignment.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:filelineiter",
        "//ortools/graph:ebert_graph",
        "//ortools/graph:linear_assignment",
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
    hdrs = ["fap_parser.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "fap_model_printer",
    hdrs = ["fap_model_printer.h"],
    deps = [
        ":fap_parser",
        "//ortools/base",
        "//ortools/base:file",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "fap_utilities",
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
    name = "golomb_sat",
    srcs = ["golomb_sat.cc"],
    deps = [
        "//ortools/base",
        "@com_google_absl//absl/strings",
        "//ortools/sat:cp_model",
        "//ortools/sat:model",
    ],
)

cc_binary(
    name = "integer_programming",
    srcs = ["integer_programming.cc"],
    deps = [
        "//ortools/base",
        "//ortools/linear_solver",
    ],
)

cc_binary(
    name = "jobshop_sat",
    srcs = [
        "jobshop_sat.cc",
    ],
    deps = [
        "//ortools/base",
        "//ortools/base:file",
        "@com_google_absl//absl/strings",
        "//ortools/base:timer",
        "//ortools/sat:cp_model",
        "//ortools/sat:cp_model_solver",
        "//ortools/sat:disjunctive",
        "//ortools/sat:integer",
        "//ortools/sat:intervals",
        "//ortools/sat:model",
        "//ortools/sat:optimization",
        "//ortools/sat:precedences",
        "//ortools/sat:sat_solver",
        "//ortools/scheduling:jobshop_scheduling_cc_proto",
        "//ortools/scheduling:jobshop_scheduling_parser",
    ],
)

cc_binary(
    name = "linear_assignment_api",
    srcs = ["linear_assignment_api.cc"],
    deps = [
        "//ortools/base",
        "//ortools/graph:ebert_graph",
        "//ortools/graph:linear_assignment",
    ],
)

cc_binary(
    name = "linear_programming",
    srcs = ["linear_programming.cc"],
    deps = [
        "//ortools/base",
        "//ortools/linear_solver",
        "//ortools/linear_solver:linear_solver_cc_proto",
    ],
)

cc_binary(
    name = "linear_solver_protocol_buffers",
    srcs = ["linear_solver_protocol_buffers.cc"],
    deps = [
        "//ortools/base",
        "//ortools/linear_solver",
        "//ortools/linear_solver:linear_solver_cc_proto",
    ],
)

cc_binary(
    name = "magic_sequence_sat",
    srcs = ["magic_sequence_sat.cc"],
    deps = [
        "//ortools/base",
        "@com_google_absl//absl/strings",
        "//ortools/sat:cp_model",
        "//ortools/sat:model",
    ],
)

cc_binary(
    name = "magic_square_sat",
    srcs = ["magic_square_sat.cc"],
    deps = [
        "//ortools/base",
        "@com_google_absl//absl/strings",
        "//ortools/sat:cp_model",
        "//ortools/sat:model",
    ],
)

cc_binary(
    name = "max_flow",
    srcs = ["max_flow.cc"],
    deps = [
        "//ortools/base",
        "//ortools/graph:max_flow",
    ],
)

cc_binary(
    name = "min_cost_flow",
    srcs = ["min_cost_flow.cc"],
    deps = [
        "//ortools/base",
        "//ortools/graph:min_cost_flow",
    ],
)

cc_binary(
    name = "mps_driver",
    srcs = ["mps_driver.cc"],
    deps = [
        "//ortools/base",
        "@com_google_absl//absl/strings",
        "//ortools/base:timer",
        "//ortools/glop:lp_solver",
        "//ortools/glop:parameters_cc_proto",
        "//ortools/lp_data:mps_reader",
        "//ortools/util:proto_tools",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_binary(
    name = "multi_knapsack_sat",
    srcs = ["multi_knapsack_sat.cc"],
    deps = [
        "//ortools/base",
        "@com_google_absl//absl/strings",
        "//ortools/sat:cp_model",
        "//ortools/sat:model",
    ],
)

cc_binary(
    name = "network_routing_sat",
    srcs = ["network_routing_sat.cc"],
    deps = [
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/base:random",
        "@com_google_absl//absl/strings",
        "//ortools/graph:shortestpaths",
        "//ortools/sat:cp_model",
        "//ortools/sat:model",
        "//ortools/util:tuple_set",
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
        "@com_google_absl//absl/strings",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_flags",
    ],
)

cc_binary(
    name = "random_tsp",
    srcs = ["random_tsp.cc"],
    deps = [
        "//ortools/base",
        "//ortools/base:random",
        "@com_google_absl//absl/strings",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_flags",
        "@com_google_protobuf//:protobuf",
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
        "//ortools/algorithms:sparse_permutation",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:filelineiter",
        "//ortools/base:random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "//ortools/base:threadpool",
        "//ortools/lp_data:mps_reader",
        "//ortools/lp_data:proto_utils",
        "//ortools/sat:boolean_problem",
        "//ortools/sat:boolean_problem_cc_proto",
        "//ortools/sat:cp_model_cc_proto",
        "//ortools/sat:cp_model_solver",
        "//ortools/sat:drat_proof_handler",
        "//ortools/sat:lp_utils",
        "//ortools/sat:optimization",
        "//ortools/sat:sat_solver",
        "//ortools/sat:simplification",
        "//ortools/sat:symmetry",
        "//ortools/util:sigint",
        "//ortools/util:time_limit",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_binary(
    name = "shift_minimization_sat",
    srcs = [
        "shift_minimization_sat.cc",
    ],
    deps = [
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:filelineiter",
        "@com_google_absl//absl/strings",
        "//ortools/sat:cp_model",
        "//ortools/sat:model",
    ],
)

cc_binary(
    name = "slitherlink_sat",
    srcs = ["slitherlink_sat.cc"],
    deps = [
        "//ortools/base",
        "//ortools/sat:cp_model",
        "//ortools/sat:model",
    ],
)

cc_binary(
    name = "solve",
    srcs = ["solve.cc"],
    deps = [
        "//ortools/base",
        "//ortools/linear_solver",
        "//ortools/linear_solver:linear_solver_cc_proto",
        "//ortools/lp_data:model_reader",
        "//ortools/lp_data:mps_reader",
    ],
)

cc_binary(
    name = "sports_scheduling_sat",
    srcs = ["sports_scheduling_sat.cc"],
    deps = [
        "//ortools/base",
        "@com_google_absl//absl/strings",
        "//ortools/sat:cp_model",
        "//ortools/sat:model",
    ],
)

cc_binary(
    name = "strawberry_fields_with_column_generation",
    srcs = ["strawberry_fields_with_column_generation.cc"],
    deps = [
        "//ortools/base",
        "//ortools/linear_solver",
    ],
)

cc_binary(
    name = "uncapacitated_facility_location",
    srcs = ["uncapacitated_facility_location.cc"],
    deps = [
        "//ortools/base",
        "//ortools/linear_solver",
    ],
)


cc_binary(
    name = "variable_intervals_sat",
    srcs = ["variable_intervals_sat.cc"],
    deps = [
        "//ortools/sat:cp_model",
        "//ortools/util:time_limit",
    ],
)

cc_binary(
    name = "vector_bin_packing_solver",
    srcs = [
        "vector_bin_packing_solver.cc",
    ],
    deps = [
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "//ortools/base:file",
        "//ortools/base",
        "//ortools/packing:arc_flow_builder",
        "//ortools/packing:arc_flow_solver",
        "//ortools/packing:vector_bin_packing_cc_proto",
        "//ortools/packing:vector_bin_packing_parser",
    ],
)

cc_binary(
    name = "weighted_tardiness_sat",
    srcs = [
        "weighted_tardiness_sat.cc",
    ],
    deps = [
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:filelineiter",
        "@com_google_absl//absl/strings",
        "//ortools/sat:cp_model",
        "//ortools/sat:cp_model_solver",
        "//ortools/sat:disjunctive",
        "//ortools/sat:integer",
        "//ortools/sat:integer_expr",
        "//ortools/sat:intervals",
        "//ortools/sat:model",
        "//ortools/sat:optimization",
        "//ortools/sat:precedences",
        "//ortools/sat:sat_solver",
        "@com_google_protobuf//:protobuf",
    ],
)
