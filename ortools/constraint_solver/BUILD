package(default_visibility = ["//visibility:public"])

proto_library(
    name = "assignment_proto",
    srcs = ["assignment.proto"],
)

cc_proto_library(
    name = "assignment_cc_proto",
    deps = [":assignment_proto"],
)

proto_library(
    name = "demon_profiler_proto",
    srcs = ["demon_profiler.proto"],
)

cc_proto_library(
    name = "demon_profiler_cc_proto",
    deps = [":demon_profiler_proto"],
)

proto_library(
    name = "model_proto",
    srcs = ["model.proto"],
    deps = [":search_limit_proto"],
)

cc_proto_library(
    name = "model_cc_proto",
    deps = [":model_proto"],
)

proto_library(
    name = "search_limit_proto",
    srcs = ["search_limit.proto"],
)

cc_proto_library(
    name = "search_limit_cc_proto",
    deps = [":search_limit_proto"],
)

proto_library(
    name = "solver_parameters_proto",
    srcs = ["solver_parameters.proto"],
)

cc_proto_library(
    name = "solver_parameters_cc_proto",
    deps = ["solver_parameters_proto"],
)

cc_library(
    name = "cp",
    srcs = [
        "ac4_mdd_reset_table.cc",
        "ac4r_table.cc",
        "alldiff_cst.cc",
        "assignment.cc",
        "collect_variables.cc",
        "constraint_solver.cc",
        "constraints.cc",
        "count_cst.cc",
        "default_search.cc",
        "demon_profiler.cc",
        "deviation.cc",
        "diffn.cc",
        "element.cc",
        "expr_array.cc",
        "expr_cst.cc",
        "expressions.cc",
        "gcc.cc",
        "graph_constraints.cc",
        "interval.cc",
        "io.cc",
        "local_search.cc",
        "model_cache.cc",
        "nogoods.cc",
        "pack.cc",
        "range_cst.cc",
        "resource.cc",
        "sat_constraint.cc",
        "sched_constraints.cc",
        "sched_expr.cc",
        "sched_search.cc",
        "search.cc",
        "table.cc",
        "timetabling.cc",
        "trace.cc",
        "tree_monitor.cc",
        "utilities.cc",
        "visitor.cc",
    ],
    hdrs = [
        "constraint_solver.h",
        "constraint_solveri.h",
        "sat_constraint.h",
    ],
    copts = [
        "-Wno-sign-compare",
        "-Wno-deprecated",
    ],
    linkopts = ["-lz"],
    deps = [
        ":assignment_cc_proto",
        ":demon_profiler_cc_proto",
        ":model_cc_proto",
        ":search_limit_cc_proto",
        ":solver_parameters_cc_proto",
        "//ortools/base",
        "//ortools/base:callback",
        "//ortools/base:file",
        "//ortools/base:hash",
        "//ortools/base:int_type",
        "//ortools/base:int_type_indexed_vector",
        "//ortools/base:map_util",
        "//ortools/base:mathutil",
        "//ortools/base:random",
        "//ortools/base:recordio",
        "//ortools/base:small_map",
        "//ortools/base:status",
        "//ortools/base:stl_util",
        "//ortools/base:strings",
        #        "//ortools/base:strings:human_readable",
        "//ortools/graph:hamiltonian_path",
        "//ortools/sat:sat_solver",
        "//ortools/util:bitset",
        "//ortools/util:cached_log",
        "//ortools/util:iterators",
        "//ortools/util:monoid_operation_tree",
        "//ortools/util:piecewise_linear_function",
        "//ortools/util:range_minimum_query",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:sorted_interval_list",
        "//ortools/util:string_array",
        "//ortools/util:tuple_set",
        "//ortools/util:vector_map",
        "//ortools/util:xml_helper",
        "//ortools/base:bitmap",
        #       "//ortools/util/regexp/re2",
        #       "//ortools/util/textprogressbar",
        #       "//ortools/util/time:clock",
        #       "//ortools/util/time:datetime",
    ],
)

# Routing

cc_proto_library(
    name = "routing_enums_cc_proto",
    deps = ["routing_enums_proto"],
)

proto_library(
    name = "routing_enums_proto",
    srcs = ["routing_enums.proto"],
)

cc_proto_library(
    name = "routing_parameters_cc_proto",
    deps = ["routing_parameters_proto"],
)

proto_library(
    name = "routing_parameters_proto",
    srcs = ["routing_parameters.proto"],
    deps = [
        ":routing_enums_proto",
        ":solver_parameters_proto",
    ],
)

cc_library(
    name = "routing_flags",
    srcs = [
        "routing_flags.cc",
    ],
    hdrs = ["routing_flags.h"],
    copts = [
        "-Wno-sign-compare",
        "-Wno-deprecated",
    ],
    deps = [
        ":cp",
        ":routing_parameters_cc_proto",
        "//ortools/base",
        "//ortools/base:map_util",
    ],
)

cc_library(
    name = "routing_types",
    hdrs = ["routing_types.h"],
    copts = [
        "-Wno-sign-compare",
        "-Wno-deprecated",
    ],
    deps = [
        "//ortools/base",
        "//ortools/base:int_type",
    ],
)

cc_library(
    name = "routing_neighborhoods",
    srcs = ["routing_neighborhoods.cc"],
    hdrs = ["routing_neighborhoods.h"],
    copts = [
        "-Wno-sign-compare",
        "-Wno-deprecated",
    ],
    deps = [
        ":cp",
        ":routing_types",
    ],
)

cc_library(
    name = "routing",
    srcs = [
        "routing.cc",
        "routing_search.cc",
    ],
    hdrs = ["routing.h"],
    copts = [
        "-Wno-sign-compare",
        "-Wno-deprecated",
    ],
    deps = [
        ":model_cc_proto",
        ":routing_neighborhoods",
        ":routing_parameters_cc_proto",
        ":routing_types",
        "//ortools/base",
        "//ortools/base:adjustable_priority_queue",
        "//ortools/base:hash",
        "//ortools/base:int_type_indexed_vector",
        "//ortools/base:map_util",
        "//ortools/base:small_map",
        "//ortools/base:small_ordered_set",
        "//ortools/base:stl_util",
        "//ortools/base:strings",
        "//ortools/constraint_solver:cp",
        "//ortools/graph",
        "//ortools/graph:christofides",
        "//ortools/graph:connectivity",
        "//ortools/graph:linear_assignment",
        "//ortools/util:bitset",
        "//ortools/util:range_query_function",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:sorted_interval_list",
        "@com_google_protobuf_cc//:protobuf",
    ],
)
