# Home of constraint solver constraint_solver

load("@rules_cc//cc:defs.bzl", "cc_proto_library")

package(
    default_visibility = ["//visibility:public"],
)

proto_library(
    name = "assignment_proto",
    srcs = ["assignment.proto"],
)

cc_proto_library(
    name = "assignment_cc_proto",
    deps = [":assignment_proto"],
)

proto_library(
    name = "search_stats_proto",
    srcs = ["search_stats.proto"],
)

cc_proto_library(
    name = "search_stats_cc_proto",
    deps = [":search_stats_proto"],
)

java_proto_library(
    name = "search_stats_java_proto",
    deps = [":search_stats_proto"],
)

proto_library(
    name = "search_limit_proto",
    srcs = ["search_limit.proto"],
)

cc_proto_library(
    name = "search_limit_cc_proto",
    deps = [":search_limit_proto"],
)

java_proto_library(
    name = "search_limit_java_proto",
    deps = [":search_limit_proto"],
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
    name = "solver_parameters_proto",
    srcs = ["solver_parameters.proto"],
)

cc_proto_library(
    name = "solver_parameters_cc_proto",
    deps = ["solver_parameters_proto"],
)

java_proto_library(
    name = "solver_parameters_java_proto",
    deps = [":solver_parameters_proto"],
)

#py_proto_library(
#    name = "assignment_py_pb2",
#    deps = [":assignment_proto"],
#)

#py_proto_library(
#    name = "search_limit_py_pb2",
#    deps = [":search_limit_proto"],
#)

#py_proto_library(
#    name = "demon_profiler_py_pb2",
#    deps = [":demon_profiler_proto"],
#)

#py_proto_library(
#    name = "solver_parameters_py_pb2",
#    deps = [":solver_parameters_proto"],
#)

# ----- Constraint Programming core engine -----

# This is the main library.
cc_library(
    name = "cp",
    srcs = [
        "alldiff_cst.cc",
        "assignment.cc",
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
        "graph_constraints.cc",
        "interval.cc",
        "local_search.cc",
        "model_cache.cc",
        "pack.cc",
        "range_cst.cc",
        "resource.cc",
        "sched_constraints.cc",
        "sched_expr.cc",
        "sched_search.cc",
        "search.cc",
        "table.cc",
        "timetabling.cc",
        "trace.cc",
        "utilities.cc",
        "visitor.cc",
    ],
    hdrs = [
        "constraint_solver.h",
        "constraint_solveri.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":assignment_cc_proto",
        ":demon_profiler_cc_proto",
        ":search_stats_cc_proto",
        ":search_limit_cc_proto",
        ":solver_parameters_cc_proto",
        ":routing_parameters_cc_proto",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:recordio",
        "//ortools/base:sysinfo",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        #        "//zlib:zlibonly",
        "//ortools/base:bitmap",
        "//ortools/base:int_type",
        "//ortools/base:strong_vector",
        "//ortools/base:iterator_adaptors",
        "//ortools/base:map_util",
        "//ortools/base:stl_util",
        #        "//util/gzip:gzipstring",
        "//ortools/base:hash",
        #        "//util/hash:jenkins",
        #        "//util/math:fastmath",
        "//ortools/base:mathutil",
        "//ortools/graph:hamiltonian_path",
        "//ortools/util:bitset",
        "//ortools/util:cached_log",
        "//ortools/util:monoid_operation_tree",
        "//ortools/util:piecewise_linear_function",
        "//ortools/util:range_minimum_query",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:sorted_interval_list",
        "//ortools/util:string_array",
        "//ortools/util:tuple_set",
        "//ortools/util:vector_map",
        "//ortools/base:random",
        #        "//util/regexp/re2",
        "@com_google_absl//absl/status",
        #        "//util/textprogressbar",
        "//ortools/base:timer",
        #        "//util/zippy",
    ],
)

# ----- Routing and ArcRouting -----

proto_library(
    name = "routing_enums_proto",
    srcs = ["routing_enums.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "routing_enums_cc_proto",
    deps = [":routing_enums_proto"],
)

java_proto_library(
    name = "routing_enums_java_proto",
    deps = [":routing_enums_proto"],
)

proto_library(
    name = "routing_parameters_proto",
    srcs = ["routing_parameters.proto"],
    deps = [
        ":routing_enums_proto",
        ":solver_parameters_proto",
        "//ortools/sat:sat_parameters_proto",
        "//ortools/util:optional_boolean_proto",
        "@com_google_protobuf//:duration_proto",
    ],
)

cc_proto_library(
    name = "routing_parameters_cc_proto",
    deps = [":routing_parameters_proto"],
)

java_proto_library(
    name = "routing_parameters_java_proto",
    deps = [":routing_parameters_proto"],
)

#py_proto_library(
#    name = "routing_parameters_py_pb2",
#    deps = [":routing_parameters_proto"],
#)

cc_library(
    name = "routing_parameters",
    srcs = ["routing_parameters.cc"],
    hdrs = ["routing_parameters.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cp",
        ":routing_enums_cc_proto",
        ":routing_parameters_cc_proto",
        ":solver_parameters_cc_proto",
        "//ortools/base",
        "//ortools/base:protoutil",
        "//ortools/util:optional_boolean_cc_proto",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "routing_flags",
    srcs = [
        "routing_flags.cc",
    ],
    hdrs = ["routing_flags.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cp",
        ":routing_enums_cc_proto",
        ":routing_parameters",
        ":routing_parameters_cc_proto",
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/base:protoutil",
        "//ortools/util:optional_boolean_cc_proto",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "routing_types",
    hdrs = ["routing_types.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ortools/base",
        "//ortools/base:int_type",
    ],
)

cc_library(
    name = "routing_index_manager",
    srcs = ["routing_index_manager.cc"],
    hdrs = ["routing_index_manager.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":routing_types",
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/base:strong_vector",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "routing",
    srcs = [
        "routing.cc",
        "routing_breaks.cc",
        "routing_filters.cc",
        "routing_flow.cc",
        "routing_lp_scheduling.cc",
        "routing_neighborhoods.cc",
        "routing_sat.cc",
        "routing_search.cc",
    ],
    hdrs = [
        "routing.h",
        "routing_filters.h",
        "routing_lp_scheduling.h",
        "routing_neighborhoods.h",
        "routing_search.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":cp",
        ":routing_enums_cc_proto",
        ":routing_index_manager",
        ":routing_parameters",
        ":routing_parameters_cc_proto",
        ":routing_types",
        "//ortools/base",
        "@com_google_protobuf//:protobuf",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        #        "//kdtree",
        "//ortools/graph",
        "//ortools/base:strong_vector",
        "//ortools/base:map_util",
        "//ortools/base:small_map",
        "//ortools/base:small_ordered_set",
        "//ortools/base:stl_util",
        "//ortools/base:hash",
        "//ortools/base:murmur",
        "//ortools/glop:lp_solver",
        "//ortools/graph:christofides",
        "//ortools/graph:connected_components",
        "//ortools/graph:linear_assignment",
        "//ortools/graph:min_cost_flow",
        "//ortools/graph:topologicalsorter",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/sat:boolean_problem",
        "//ortools/sat:cp_constraints",
        "//ortools/sat:cp_model",
        "//ortools/sat:cp_model_cc_proto",
        "//ortools/sat:cp_model_checker",
        "//ortools/sat:cp_model_solver",
        "//ortools/sat:cp_model_utils",
        "//ortools/sat:integer",
        "//ortools/sat:integer_expr",
        "//ortools/sat:model",
        "//ortools/sat:optimization",
        "//ortools/sat:theta_tree",
        "//ortools/util:bitset",
        "//ortools/util:optional_boolean_cc_proto",
        "//ortools/util:range_query_function",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:sorted_interval_list",
        "//ortools/base:adjustable_priority_queue",
        "//ortools/base:protoutil",
    ],
)
