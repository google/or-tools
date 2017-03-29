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

#proto_library(
#    name = "cp_model_proto",
#    srcs = ["cp_model.proto"],
#)

#cc_library(
#    name = "cp_model_utils",
#    srcs = ["cp_model_utils.cc"],
#    hdrs = ["cp_model_utils.h"],
#    visibility = ["//visibility:public"],
#    deps = [
#        ":cp_model_proto",
#        "//base",
#        "//util/gtl:stl_util",
#        "//util/operations_research/util:sorted_interval_list",
#    ],
#)

#cc_library(
#    name = "cp_model_checker",
#    srcs = ["cp_model_checker.cc"],
#    hdrs = ["cp_model_checker.h"],
#    visibility = ["//visibility:public"],
#    deps = [
#        ":cp_model_proto",
#        ":cp_model_utils",
#        "//base",
#        "//strings",
#        "//third_party/absl/strings",
#        "//util/gtl:map_util",
#        "//util/operations_research/util:saturated_arithmetic",
#        "//util/operations_research/util:sorted_interval_list",
#    ],
#)

#cc_library(
#    name = "cp_model_solver",
#    srcs = ["cp_model_solver.cc"],
#    hdrs = ["cp_model_solver.h"],
#    visibility = ["//visibility:public"],
#    deps = [
#        ":cp_model_checker",
#        ":cp_model_presolve",
#        ":cp_model_proto",
#        ":cp_model_utils",
#        ":cumulative",
#        ":disjunctive",
#        ":integer",
#        ":intervals",
#        ":linear_programming_constraint",
#        ":model",
#        ":optimization",
#        ":sat_solver",
#        ":table",
#        "//base",
#        "//strings",
#        "//third_party/absl/strings",
#        "//util/gtl:stl_util",
#        "//util/operations_research/graph:connectivity",
#    ],
#)

#cc_library(
#    name = "cp_model_presolve",
#    srcs = ["cp_model_presolve.cc"],
#    hdrs = ["cp_model_presolve.h"],
#    visibility = ["//visibility:public"],
#    deps = [
#        ":cp_model_checker",
#        ":cp_model_proto",
#        ":cp_model_utils",
#        "//base",
#        "//strings",
#        "//util/gtl:map_util",
#        "//util/gtl:stl_util",
#        "//util/operations_research/util:bitset",
#        "//util/operations_research/util:sorted_interval_list",
#    ],
#)

cc_library(
    name = "model",
    hdrs = ["model.h"],
    deps = [
        "//base",
        "//base:map_util",
        "//base:typeid",
    ],
)

cc_library(
    name = "sat_base",
    hdrs = ["sat_base.h"],
    deps = [
        ":model",
        "//base",
        "//base:int_type",
        "//base:int_type_indexed_vector",
        "//base:strings",
        "//util:bitset",
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
        "//base",
        "//base:adjustable_priority_queue",
        "//base:int_type",
        "//base:int_type_indexed_vector",
        "//base:map_util",
        "//base:random",
        "//base:stl_util",
        "//base:strings",
        "//util:bitset",
        "//util:running_stat",
        "//util:saturated_arithmetic",
        "//util:stats",
        "//util:time_limit",
        #        "//util/time:clock",
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
        "//base",
        "//base:hash",
        #        "//base:inlined_vector",
        "//base:int_type",
        "//base:int_type_indexed_vector",
        "//base:stl_util",
        "//base:strings",
        "//util:bitset",
        "//util:stats",
        "//util:time_limit",
        "//base:random",
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
        "//algorithms:dynamic_partition",
        "//base",
        "//base:adjustable_priority_queue",
        "//base:random",
        "//base:stl_util",
        "//base:strongly_connected_components",
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
        "//base:hash",
        "//util:saturated_arithmetic",
        "//util:stats",
    ],
)

cc_library(
    name = "symmetry",
    srcs = ["symmetry.cc"],
    hdrs = ["symmetry.h"],
    copts = [W_FLOAT_CONVERSION],
    deps = [
        ":sat_base",
        "//algorithms:sparse_permutation",
        "//base:hash",
        "//util:stats",
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
        "//base:stl_util",
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
        "//base",
        #        "//base:inlined_vector",
        "//base:int_type",
        #        "//base:iterator_adaptors",
        "//base:map_util",
        "//base:stl_util",
        "//base:strings",
        "//util:bitset",
        "//util:iterators",
        "//util:rev",
        "//util:saturated_arithmetic",
        "//util:sorted_interval_list",
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
        "//util:sort",
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
        "//base:cleanup",
        "//base:stl_util",
        "//util:bitset",
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
        #        "//base:iterator_adaptors",
        "//util:stats",
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
        "//util:sort",
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
        "//base:int_type",
        "//util:sort",
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
        "//base",
        #        "//base:iterator_adaptors",
        "//util:sort",
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
        "//algorithms:find_graph_symmetries",
        "//algorithms:sparse_permutation",
        "//base",
        "//base:hash",
        "//base:map_util",
        "//base:status",
        "//base:strings",
        "//graph",
        "//graph:io",
        "//graph:util",
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
        "//base",
        "//glop",
        "//lp_data",
        "//util:time_limit",
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
        #        ":cp_model_cc_proto",
        ":sat_solver",
        "//base:strings",
        "//glop",
        "//linear_solver:linear_solver_cc_proto",
        "//lp_data",
        "//lp_data:lp_print_utils",
        "//util:fp_utils",
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
        ":model",
        ":sat_solver",
        ":util",
        "//base",
        "//base:strings",
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
        "//base",
        "//base:random",
        "//base:strings",
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
        "//base",
        "//base:map_util",
        "//base:stl_util",
        "//base:strings",
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
        "//base",
        "//base:map_util",
        "//util:sort",
        "//util:sorted_interval_list",
    ],
)

cc_library(
    name = "flow_costs",
    srcs = ["flow_costs.cc"],
    hdrs = ["flow_costs.h"],
    deps = [
        ":integer",
        ":model",
        ":sat_base",
        "//linear_solver:linear_solver_glop",
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
        "//base",
        "//base:strings",
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
        "//base",
        "//base:file",
        "//base:hash",
        "//base:strings",
    ],
)
