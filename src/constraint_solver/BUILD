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
        "//base",
        "//base:callback",
        "//base:file",
        "//base:hash",
        "//base:int_type",
        "//base:int_type_indexed_vector",
        "//base:map_util",
        "//base:mathutil",
        "//base:random",
        "//base:recordio",
        "//base:small_map",
        "//base:status",
        "//base:stl_util",
        "//base:strings",
        #        "//base:strings:human_readable",
        "//graph:hamiltonian_path",
        "//sat:sat_solver",
        "//util:bitset",
        "//util:cached_log",
        "//util:iterators",
        "//util:monoid_operation_tree",
        "//util:piecewise_linear_function",
        "//util:range_minimum_query",
        "//util:saturated_arithmetic",
        "//util:sorted_interval_list",
        "//util:string_array",
        "//util:tuple_set",
        "//util:vector_map",
        "//util:xml_helper",
        "//base:bitmap",
        #       "//util/regexp/re2",
        #       "//util/textprogressbar",
        #       "//util/time:clock",
        #       "//util/time:datetime",
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
        "//base",
        "//base:map_util",
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
        "//base",
        "//base:int_type",
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
        "//base",
        "//base:adjustable_priority_queue",
        "//base:hash",
        "//base:int_type_indexed_vector",
        "//base:map_util",
        "//base:small_map",
        "//base:small_ordered_set",
        "//base:stl_util",
        "//base:strings",
        "//constraint_solver:cp",
        "//graph",
        "//graph:christofides",
        "//graph:connectivity",
        "//graph:linear_assignment",
        "//util:bitset",
        "//util:range_query_function",
        "//util:saturated_arithmetic",
        "//util:sorted_interval_list",
        "@com_google_protobuf_cc//:protobuf",
    ],
)
