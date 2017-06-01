# Description:
# Home of SAT solver
package(
    default_visibility = ["//visibility:public"],
)

proto_library(
    name = "sat_parameters_proto",
    srcs = ["sat_parameters.proto"],
)

cc_proto_library(
    name = "sat_parameters_cc_proto",
    deps = [":sat_parameters_proto"],
)

cc_proto_library(
    name = "boolean_problem_cc_proto",
    deps = [":boolean_problem_proto"],
)

proto_library(
    name = "boolean_problem_proto",
    srcs = ["boolean_problem.proto"],
)

cc_proto_library(
    name = "cp_model_cc_proto",
    deps = [":cp_model_proto"],
)

proto_library(
    name = "cp_model_proto",
    srcs = ["cp_model.proto"],
)

cc_library(
    name = "cp_model_utils",
    srcs = ["cp_model_utils.cc"],
    hdrs = ["cp_model_utils.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cp_model_cc_proto",
        "//ortools/base",
        "//ortools/base:stl_util",
        "//ortools/util:sorted_interval_list",
    ],
)

cc_library(
    name = "cp_model_checker",
    srcs = ["cp_model_checker.cc"],
    hdrs = ["cp_model_checker.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_utils",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:strings",
        "//ortools/base:map_util",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:sorted_interval_list",
    ],
)

cc_library(
    name = "cp_model_solver",
    srcs = ["cp_model_solver.cc"],
    hdrs = ["cp_model_solver.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cp_model_checker",
        ":cp_model_presolve",
        ":cp_model_cc_proto",
        ":cp_model_utils",
        ":cumulative",
        ":disjunctive",
        ":integer",
        ":intervals",
        ":linear_programming_constraint",
        ":model",
        ":optimization",
        ":sat_solver",
        ":table",
        "//ortools/base",
        "//ortools/base:strings",
        "//ortools/base:hash",
        "//ortools/base:stl_util",
        "//ortools/graph:connectivity",
    ],
)

cc_library(
    name = "cp_model_presolve",
    srcs = ["cp_model_presolve.cc"],
    hdrs = ["cp_model_presolve.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cp_model_checker",
        ":cp_model_cc_proto",
        ":cp_model_utils",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:strings",
        "//ortools/base:map_util",
        "//ortools/base:stl_util",
        "//ortools/util:affine_relation",
        "//ortools/util:bitset",
        "//ortools/util:sorted_interval_list",
    ],
)

cc_library(
    name = "model",
    hdrs = ["model.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/base:typeid",
    ],
)

cc_library(
    name = "sat_base",
    hdrs = ["sat_base.h"],
    deps = [
        ":model",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:int_type_indexed_vector",
        "//ortools/base:span",
        "//ortools/base:strings",
        "//ortools/util:bitset",
    ],
)

# W_FLOAT_CONVERSION = "-Xclang-only=-Wfloat-conversion"
W_FLOAT_CONVERSION = ""

cc_library(
    name = "sat_solver",
    srcs = [
        "sat_solver.cc",
    ],
    hdrs = ["sat_solver.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":clause",
        ":drat",
        ":model",
        ":pb_constraint",
        ":sat_base",
        ":sat_parameters_cc_proto",
        "//ortools/base",
        "//ortools/base:adjustable_priority_queue",
        "//ortools/base:int_type",
        "//ortools/base:int_type_indexed_vector",
        "//ortools/base:map_util",
        "//ortools/base:random",
        "//ortools/base:stl_util",
        "//ortools/base:strings",
        "//ortools/util:bitset",
        "//ortools/util:running_stat",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:stats",
        "//ortools/util:time_limit",
        #        "//ortools/util/time:clock",
        "@com_google_protobuf_cc//:protobuf",
    ],
)

cc_library(
    name = "clause",
    srcs = ["clause.cc"],
    hdrs = ["clause.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":sat_base",
        ":sat_parameters_cc_proto",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:inlined_vector",
        "//ortools/base:int_type",
        "//ortools/base:int_type_indexed_vector",
        "//ortools/base:span",
        "//ortools/base:stl_util",
        "//ortools/base:strings",
        "//ortools/util:bitset",
        "//ortools/util:stats",
        "//ortools/util:time_limit",
        "//ortools/base:random",
    ],
)

cc_library(
    name = "simplification",
    srcs = ["simplification.cc"],
    hdrs = ["simplification.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":drat",
        ":sat_base",
        ":sat_parameters_cc_proto",
        ":sat_solver",
        ":util",
        "//ortools/algorithms:dynamic_partition",
        "//ortools/base",
        "//ortools/base:adjustable_priority_queue",
        "//ortools/base:random",
        "//ortools/base:stl_util",
        "//ortools/base:strongly_connected_components",
    ],
)

cc_library(
    name = "pb_constraint",
    srcs = ["pb_constraint.cc"],
    hdrs = ["pb_constraint.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":sat_base",
        ":sat_parameters_cc_proto",
        "//ortools/base:hash",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "symmetry",
    srcs = ["symmetry.cc"],
    hdrs = ["symmetry.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":sat_base",
        "//ortools/algorithms:sparse_permutation",
        "//ortools/base:hash",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "no_cycle",
    srcs = ["no_cycle.cc"],
    hdrs = ["no_cycle.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":model",
        ":sat_base",
        ":sat_solver",
        "//ortools/base:stl_util",
    ],
)

cc_library(
    name = "integer",
    srcs = ["integer.cc"],
    hdrs = ["integer.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":model",
        ":sat_base",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:inlined_vector",
        "//ortools/base:int_type",
        "//ortools/base:iterator_adaptors",
        "//ortools/base:map_util",
        "//ortools/base:span",
        "//ortools/base:stl_util",
        "//ortools/base:strings",
        "//ortools/util:bitset",
        "//ortools/util:iterators",
        "//ortools/util:rev",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:sorted_interval_list",
    ],
)

cc_library(
    name = "intervals",
    srcs = ["intervals.cc"],
    hdrs = ["intervals.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":cp_constraints",
        ":integer",
        ":integer_expr",
        ":model",
        ":precedences",
        ":sat_base",
        ":sat_solver",
        "//ortools/util:sort",
    ],
)

cc_library(
    name = "precedences",
    srcs = ["precedences.cc"],
    hdrs = ["precedences.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":integer",
        ":model",
        ":sat_base",
        ":sat_solver",
        "//ortools/base:cleanup",
        "//ortools/base:stl_util",
        "//ortools/util:bitset",
    ],
)

cc_library(
    name = "integer_expr",
    srcs = ["integer_expr.cc"],
    hdrs = ["integer_expr.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":integer",
        ":model",
        ":precedences",
        ":sat_base",
    ],
)

cc_library(
    name = "theta_tree",
    srcs = ["theta_tree.cc"],
    hdrs = ["theta_tree.h"],
    deps = [
        ":integer",
    ],
)

cc_library(
    name = "disjunctive",
    srcs = ["disjunctive.cc"],
    hdrs = ["disjunctive.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":cp_constraints",
        ":integer",
        ":intervals",
        ":model",
        ":precedences",
        ":sat_base",
        ":sat_solver",
        ":theta_tree",
        "//ortools/base:iterator_adaptors",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "timetable",
    srcs = ["timetable.cc"],
    hdrs = ["timetable.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":integer",
        ":intervals",
        ":model",
        ":overload_checker",
        ":precedences",
        ":sat_base",
        ":sat_solver",
        "//ortools/util:sort",
    ],
)

cc_library(
    name = "timetable_edgefinding",
    srcs = ["timetable_edgefinding.cc"],
    hdrs = ["timetable_edgefinding.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":integer",
        ":intervals",
        ":model",
        ":sat_base",
        ":sat_solver",
        "//ortools/base:int_type",
        "//ortools/util:sort",
    ],
)

cc_library(
    name = "cumulative",
    srcs = ["cumulative.cc"],
    hdrs = ["cumulative.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":disjunctive",
        ":integer",
        ":intervals",
        ":model",
        ":overload_checker",
        ":sat_base",
        ":sat_solver",
        ":timetable",
        ":timetable_edgefinding",
    ],
)

cc_library(
    name = "overload_checker",
    srcs = ["overload_checker.cc"],
    hdrs = ["overload_checker.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":integer",
        ":intervals",
        ":model",
        ":sat_base",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:iterator_adaptors",
        "//ortools/util:sort",
    ],
)

cc_library(
    name = "boolean_problem",
    srcs = ["boolean_problem.cc"],
    hdrs = ["boolean_problem.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":boolean_problem_cc_proto",
        ":sat_solver",
        ":simplification",
        "//ortools/algorithms:find_graph_symmetries",
        "//ortools/algorithms:sparse_permutation",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/base:status",
        "//ortools/base:strings",
        "//ortools/graph",
        "//ortools/graph:io",
        "//ortools/graph:util",
    ],
)

cc_library(
    name = "linear_programming_constraint",
    srcs = ["linear_programming_constraint.cc"],
    hdrs = ["linear_programming_constraint.h"],
    deps = [
        ":integer",
        ":model",
        ":sat_base",
        "//ortools/base",
        "//ortools/glop",
        "//ortools/lp_data",
        "//ortools/util:time_limit",
    ],
)

cc_library(
    name = "lp_utils",
    srcs = ["lp_utils.cc"],
    hdrs = ["lp_utils.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":boolean_problem",
        ":boolean_problem_cc_proto",
        ":cp_model_cc_proto",
        ":sat_solver",
        "//ortools/base:strings",
        "//ortools/glop",
        "//ortools/linear_solver:linear_solver_cc_proto",
        "//ortools/lp_data",
        "//ortools/lp_data:lp_print_utils",
        "//ortools/util:fp_utils",
    ],
)

cc_library(
    name = "optimization",
    srcs = ["optimization.cc"],
    hdrs = ["optimization.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":boolean_problem",
        ":encoding",
        ":integer",
        ":integer_expr",
        ":model",
        ":sat_solver",
        ":util",
        "//ortools/base",
        "//ortools/base:strings",
        "@com_google_protobuf_cc//:protobuf",
    ],
)

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":sat_parameters_cc_proto",
        "//ortools/base",
        "//ortools/base:random",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "table",
    srcs = ["table.cc"],
    hdrs = ["table.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":integer",
        ":model",
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/base:stl_util",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "cp_constraints",
    srcs = ["cp_constraints.cc"],
    hdrs = ["cp_constraints.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":integer",
        ":model",
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/util:sort",
        "//ortools/util:sorted_interval_list",
    ],
)

cc_library(
    name = "flow_costs",
    srcs = ["flow_costs.cc"],
    hdrs = ["flow_costs.h"],
    copts = ["-DUSE_GLOP"],
    deps = [
        ":integer",
        ":model",
        ":sat_base",
        "//ortools/linear_solver",
    ],
)

cc_library(
    name = "encoding",
    srcs = ["encoding.cc"],
    hdrs = ["encoding.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":boolean_problem_cc_proto",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "drat",
    srcs = ["drat.cc"],
    hdrs = ["drat.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":model",
        ":sat_base",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:hash",
        "//ortools/base:strings",
    ],
)
