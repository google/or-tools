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
    deps = [
        ":assignment_cc_proto",
        ":demon_profiler_cc_proto",
        ":model_cc_proto",
        #        ":search_limit_cc_proto",
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
