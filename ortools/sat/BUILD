# Home of CP/SAT solver (which includes SAT, max-SAT and PB problems).
#

load("@rules_cc//cc:defs.bzl", "cc_proto_library")

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
    name = "cp_model",
    srcs = ["cp_model.cc"],
    hdrs = ["cp_model.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_solver",
        ":cp_model_utils",
        ":model",
        ":sat_parameters_cc_proto",
        "//ortools/base:map_util",
        "//ortools/util:sorted_interval_list",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "model",
    hdrs = ["model.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/base:typeid",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
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
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "synchronization",
    srcs = ["synchronization.cc"],
    hdrs = ["synchronization.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_mapping",
        ":cp_model_utils",
        ":integer",
        ":linear_programming_constraint",
        ":model",
        ":sat_base",
        "//ortools/base",
        "//ortools/util:bitset",
        "//ortools/util:logging",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/synchronization",
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
        "//ortools/base:map_util",
        "//ortools/port:proto_utils",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:sorted_interval_list",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "parameters_validation",
    srcs = ["parameters_validation.cc"],
    hdrs = ["parameters_validation.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":sat_parameters_cc_proto",
        "//ortools/base",
    ],
)

cc_library(
    name = "cp_model_search",
    srcs = ["cp_model_search.cc"],
    hdrs = ["cp_model_search.h"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_mapping",
        ":cp_model_utils",
        ":integer",
        ":integer_search",
        ":model",
        ":probing",
        ":util",
        "//ortools/base",
        "//ortools/base:cleanup",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "cp_model_objective",
    srcs = ["cp_model_objective.cc"],
    hdrs = ["cp_model_objective.h"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_utils",
        "//ortools/base",
    ],
)

cc_library(
    name = "cp_model_solver",
    srcs = ["cp_model_solver.cc"],
    hdrs = ["cp_model_solver.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":circuit",
        ":clause",
        ":cp_model_cc_proto",
        ":cp_model_checker",
        ":cp_model_expand",
        ":cp_model_lns",
        ":cp_model_loader",
        ":cp_model_postsolve",
        ":cp_model_presolve",
        ":cp_model_search",
        ":cp_model_utils",
        ":drat_checker",
        ":drat_proof_handler",
        ":feasibility_pump",
        ":integer",
        ":integer_expr",
        ":integer_search",
        ":lb_tree_search",
        ":linear_programming_constraint",
        ":linear_relaxation",
        ":model",
        ":optimization",
        ":parameters_validation",
        ":precedences",
        ":probing",
        ":rins",
        ":sat_base",
        ":sat_parameters_cc_proto",
        ":sat_solver",
        ":simplification",
        ":subsolver",
        ":synchronization",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/base:stl_util",
        "//ortools/base:strong_vector",
        "//ortools/base:threadpool",
        "//ortools/graph:connected_components",
        "//ortools/port:proto_utils",
        "//ortools/util:logging",
        "//ortools/util:sigint",
        "//ortools/util:sorted_interval_list",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cp_model_mapping",
    hdrs = ["cp_model_mapping.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_utils",
        ":integer",
        ":intervals",
        ":linear_constraint",
        ":model",
        ":sat_base",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:strong_vector",
        "//ortools/util:logging",
        "//ortools/util:sorted_interval_list",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "cp_model_loader",
    srcs = ["cp_model_loader.cc"],
    hdrs = ["cp_model_loader.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":all_different",
        ":circuit",
        ":cp_constraints",
        ":cp_model_cc_proto",
        ":cp_model_mapping",
        ":cp_model_symmetries",
        ":cp_model_utils",
        ":cumulative",
        ":diffn",
        ":disjunctive",
        ":implied_bounds",
        ":integer",
        ":integer_expr",
        ":intervals",
        ":linear_relaxation",
        ":model",
        ":pb_constraint",
        ":precedences",
        ":sat_base",
        ":sat_parameters_cc_proto",
        ":sat_solver",
        ":symmetry",
        ":table",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/base:stl_util",
        "//ortools/base:strong_vector",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:sorted_interval_list",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "presolve_util",
    srcs = ["presolve_util.cc"],
    hdrs = ["presolve_util.h"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_utils",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/base:strong_vector",
        "//ortools/util:bitset",
        "//ortools/util:sorted_interval_list",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "presolve_context",
    srcs = ["presolve_context.cc"],
    hdrs = ["presolve_context.h"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_utils",
        ":model",
        ":presolve_util",
        ":sat_parameters_cc_proto",
        ":util",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/base:mathutil",
        "//ortools/base:strong_vector",
        "//ortools/port:proto_utils",
        "//ortools/util:affine_relation",
        "//ortools/util:bitset",
        "//ortools/util:logging",
        "//ortools/util:sorted_interval_list",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "cp_model_presolve",
    srcs = ["cp_model_presolve.cc"],
    hdrs = ["cp_model_presolve.h"],
    deps = [
        ":circuit",
        ":cp_model_cc_proto",
        ":cp_model_checker",
        ":cp_model_expand",
        ":cp_model_loader",
        ":cp_model_mapping",
        ":cp_model_objective",
        ":cp_model_symmetries",
        ":cp_model_utils",
        ":presolve_context",
        ":presolve_util",
        ":probing",
        ":sat_base",
        ":sat_parameters_cc_proto",
        ":simplification",
        ":var_domination",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/base:mathutil",
        "//ortools/base:stl_util",
        "//ortools/port:proto_utils",
        "//ortools/util:affine_relation",
        "//ortools/util:bitset",
        "//ortools/util:sorted_interval_list",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "cp_model_postsolve",
    srcs = [
        "cp_model_postsolve.cc",
    ],
    hdrs = ["cp_model_postsolve.h"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_utils",
        "//ortools/base",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_library(
    name = "cp_model_expand",
    srcs = ["cp_model_expand.cc"],
    hdrs = ["cp_model_expand.h"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_utils",
        ":presolve_context",
        ":util",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/util:saturated_arithmetic",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "sat_base",
    hdrs = ["sat_base.h"],
    deps = [
        ":model",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:strong_vector",
        "//ortools/util:bitset",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

# Enable a warning to check for floating point to integer conversions.
# In GCC-4.8, this was "-Wreal-conversion", but was removed in 4.9
# In Clang, this warning is "-Wfloat-conversion"

cc_library(
    name = "sat_solver",
    srcs = [
        "sat_solver.cc",
    ],
    hdrs = ["sat_solver.h"],
    deps = [
        ":clause",
        ":drat_proof_handler",
        ":model",
        ":pb_constraint",
        ":restart",
        ":sat_base",
        ":sat_decision",
        ":sat_parameters_cc_proto",
        ":util",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/base:stl_util",
        "//ortools/base:strong_vector",
        "//ortools/port:proto_utils",
        "//ortools/port:sysinfo",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:stats",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "restart",
    srcs = ["restart.cc"],
    hdrs = ["restart.h"],
    deps = [
        ":model",
        ":sat_decision",
        ":sat_parameters_cc_proto",
        "//ortools/base",
        "//ortools/port:proto_utils",
        "//ortools/util:bitset",
        "//ortools/util:running_stat",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "probing",
    srcs = ["probing.cc"],
    hdrs = ["probing.h"],
    deps = [
        ":clause",
        ":implied_bounds",
        ":integer",
        ":model",
        ":sat_solver",
        "//ortools/base",
        "//ortools/util:logging",
    ],
)

cc_library(
    name = "sat_inprocessing",
    srcs = ["sat_inprocessing.cc"],
    hdrs = ["sat_inprocessing.h"],
    deps = [
        ":clause",
        ":model",
        ":probing",
        ":sat_base",
        ":sat_solver",
        "//ortools/base",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/container:inlined_vector",
    ],
)

cc_library(
    name = "sat_decision",
    srcs = ["sat_decision.cc"],
    hdrs = ["sat_decision.h"],
    deps = [
        ":model",
        ":pb_constraint",
        ":sat_base",
        ":sat_parameters_cc_proto",
        ":util",
        "//ortools/base",
        "//ortools/util:bitset",
        "//ortools/util:integer_pq",
        "//ortools/util:random_engine",
    ],
)

cc_library(
    name = "clause",
    srcs = ["clause.cc"],
    hdrs = ["clause.h"],
    deps = [
        ":drat_proof_handler",
        ":model",
        ":sat_base",
        ":sat_parameters_cc_proto",
        ":util",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:int_type",
        "//ortools/base:stl_util",
        "//ortools/base:strong_vector",
        "//ortools/graph:strongly_connected_components",
        "//ortools/util:bitset",
        "//ortools/util:random_engine",
        "//ortools/util:stats",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "simplification",
    srcs = ["simplification.cc"],
    hdrs = ["simplification.h"],
    deps = [
        ":drat_proof_handler",
        ":sat_base",
        ":sat_inprocessing",
        ":sat_parameters_cc_proto",
        ":sat_solver",
        ":util",
        "//ortools/algorithms:dynamic_partition",
        "//ortools/base",
        "//ortools/base:adjustable_priority_queue",
        "//ortools/base:int_type",
        "//ortools/base:random",
        "//ortools/base:stl_util",
        "//ortools/base:strong_vector",
        "//ortools/graph:strongly_connected_components",
        "//ortools/util:logging",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "pb_constraint",
    srcs = ["pb_constraint.cc"],
    hdrs = ["pb_constraint.h"],
    deps = [
        ":sat_base",
        ":sat_parameters_cc_proto",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:int_type",
        "//ortools/base:murmur",
        "//ortools/base:strong_vector",
        "//ortools/util:bitset",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:stats",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "symmetry",
    srcs = ["symmetry.cc"],
    hdrs = ["symmetry.h"],
    deps = [
        ":sat_base",
        "//ortools/algorithms:sparse_permutation",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:strong_vector",
        "//ortools/util:stats",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "symmetry_util",
    srcs = ["symmetry_util.cc"],
    hdrs = ["symmetry_util.h"],
    deps = [
        "//ortools/algorithms:dynamic_partition",
        "//ortools/algorithms:sparse_permutation",
        "//ortools/base",
    ],
)

cc_library(
    name = "var_domination",
    srcs = ["var_domination.cc"],
    hdrs = ["var_domination.h"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_utils",
        ":integer",
        ":presolve_context",
        "//ortools/algorithms:dynamic_partition",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:strong_vector",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "integer",
    srcs = ["integer.cc"],
    hdrs = ["integer.h"],
    deps = [
        ":model",
        ":sat_base",
        ":sat_parameters_cc_proto",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:int_type",
        "//ortools/base:iterator_adaptors",
        "//ortools/base:map_util",
        "//ortools/base:stl_util",
        "//ortools/base:strong_vector",
        "//ortools/graph:iterators",
        "//ortools/util:bitset",
        "//ortools/util:rev",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:sorted_interval_list",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "integer_search",
    srcs = ["integer_search.cc"],
    hdrs = ["integer_search.h"],
    deps = [
        ":cp_model_mapping",
        ":disjunctive",
        ":integer",
        ":linear_programming_constraint",
        ":probing",
        ":pseudo_costs",
        ":rins",
        ":sat_base",
        ":sat_decision",
        ":sat_inprocessing",
        ":sat_solver",
        ":util",
    ],
)

cc_library(
    name = "lb_tree_search",
    srcs = ["lb_tree_search.cc"],
    hdrs = ["lb_tree_search.h"],
    deps = [
        ":cp_model_mapping",
        ":integer",
        ":integer_search",
        ":sat_base",
        ":sat_solver",
        ":synchronization",
        ":util",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "pseudo_costs",
    srcs = ["pseudo_costs.cc"],
    hdrs = ["pseudo_costs.h"],
    deps = [
        ":integer",
        ":sat_decision",
        ":sat_parameters_cc_proto",
    ],
)

cc_library(
    name = "intervals",
    srcs = ["intervals.cc"],
    hdrs = ["intervals.h"],
    deps = [
        ":cp_constraints",
        ":integer",
        ":integer_expr",
        ":model",
        ":pb_constraint",
        ":precedences",
        ":sat_base",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:strong_vector",
        "//ortools/util:sort",
    ],
)

cc_library(
    name = "precedences",
    srcs = ["precedences.cc"],
    hdrs = ["precedences.h"],
    deps = [
        ":cp_constraints",
        ":integer",
        ":model",
        ":sat_base",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:cleanup",
        "//ortools/base:int_type",
        "//ortools/base:stl_util",
        "//ortools/base:strong_vector",
        "//ortools/util:bitset",
        "@com_google_absl//absl/container:inlined_vector",
    ],
)

cc_library(
    name = "implied_bounds",
    srcs = ["implied_bounds.cc"],
    hdrs = ["implied_bounds.h"],
    deps = [
        ":integer",
        ":model",
        ":sat_base",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/base:strong_vector",
        "//ortools/util:bitset",
        "@com_google_absl//absl/container:inlined_vector",
    ],
)

cc_library(
    name = "integer_expr",
    srcs = ["integer_expr.cc"],
    hdrs = ["integer_expr.h"],
    deps = [
        ":integer",
        ":linear_constraint",
        ":model",
        ":precedences",
        ":sat_base",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:stl_util",
        "//ortools/util:sorted_interval_list",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "all_different",
    srcs = ["all_different.cc"],
    hdrs = ["all_different.h"],
    deps = [
        ":integer",
        ":model",
        ":sat_base",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/graph:strongly_connected_components",
        "//ortools/util:sort",
        "//ortools/util:sorted_interval_list",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "theta_tree",
    srcs = ["theta_tree.cc"],
    hdrs = ["theta_tree.h"],
    deps = [
        ":integer",
        "//ortools/base",
        "//ortools/base:int_type",
    ],
)

cc_library(
    name = "disjunctive",
    srcs = ["disjunctive.cc"],
    hdrs = ["disjunctive.h"],
    deps = [
        ":all_different",
        ":integer",
        ":intervals",
        ":model",
        ":precedences",
        ":sat_base",
        ":sat_parameters_cc_proto",
        ":sat_solver",
        ":theta_tree",
        ":timetable",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:iterator_adaptors",
    ],
)

cc_library(
    name = "timetable",
    srcs = ["timetable.cc"],
    hdrs = ["timetable.h"],
    deps = [
        ":integer",
        ":intervals",
        ":sat_base",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/util:rev",
        "//ortools/util:sort",
    ],
)

cc_library(
    name = "timetable_edgefinding",
    srcs = ["timetable_edgefinding.cc"],
    hdrs = ["timetable_edgefinding.h"],
    deps = [
        ":integer",
        ":intervals",
        ":sat_base",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/util:sort",
    ],
)

cc_library(
    name = "cumulative",
    srcs = ["cumulative.cc"],
    hdrs = ["cumulative.h"],
    deps = [
        ":cumulative_energy",
        ":disjunctive",
        ":integer",
        ":intervals",
        ":model",
        ":pb_constraint",
        ":precedences",
        ":sat_base",
        ":sat_parameters_cc_proto",
        ":sat_solver",
        ":timetable",
        ":timetable_edgefinding",
        "//ortools/base",
        "//ortools/base:int_type",
    ],
)

cc_library(
    name = "cumulative_energy",
    srcs = ["cumulative_energy.cc"],
    hdrs = ["cumulative_energy.h"],
    deps = [
        ":integer",
        ":intervals",
        ":sat_base",
        ":sat_solver",
        ":theta_tree",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/util:sort",
    ],
)

#cc_library(
#    name = "cumulative_energy",
#    srcs = ["cumulative_energy.cc"],
#    hdrs = ["cumulative_energy.h"],
#    deps = [
#        ":integer",
#        ":intervals",
#        ":model",
#        ":sat_base",
#        ":sat_solver",
#        ":theta_tree",
#        "//ortools/base",
#        "//ortools/base:int_type",
#        "//ortools/base:iterator_adaptors",
#    ],
#)

cc_library(
    name = "boolean_problem",
    srcs = ["boolean_problem.cc"],
    hdrs = ["boolean_problem.h"],
    deps = [
        ":boolean_problem_cc_proto",
        ":cp_model_cc_proto",
        ":pb_constraint",
        ":sat_base",
        ":sat_parameters_cc_proto",
        ":sat_solver",
        ":simplification",
        "//ortools/algorithms:find_graph_symmetries",
        "//ortools/algorithms:sparse_permutation",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/base:strong_vector",
        "//ortools/graph:io",
        "//ortools/graph:util",
        "//ortools/port:proto_utils",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "linear_relaxation",
    srcs = ["linear_relaxation.cc"],
    hdrs = ["linear_relaxation.h"],
    deps = [
        ":circuit",
        ":cp_model_mapping",
        ":integer",
        ":linear_programming_constraint",
        ":model",
        ":scheduling_constraints",
        ":scheduling_cuts",
        "//ortools/base",
        "//ortools/base:iterator_adaptors",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "linear_constraint",
    srcs = ["linear_constraint.cc"],
    hdrs = ["linear_constraint.h"],
    deps = [
        ":integer",
        ":model",
        "//ortools/base",
        "//ortools/base:mathutil",
    ],
)

cc_library(
    name = "linear_programming_constraint",
    srcs = ["linear_programming_constraint.cc"],
    hdrs = ["linear_programming_constraint.h"],
    deps = [
        ":cuts",
        ":integer",
        ":integer_expr",
        ":intervals",
        ":linear_constraint",
        ":linear_constraint_manager",
        ":model",
        ":zero_half_cuts",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/base:strong_vector",
        "//ortools/glop:parameters_cc_proto",
        "//ortools/glop:preprocessor",
        "//ortools/glop:revised_simplex",
        "//ortools/glop:status",
        "//ortools/graph:strongly_connected_components",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:matrix_scaler",
        "//ortools/util:rev",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/numeric:int128",
    ],
)

cc_library(
    name = "linear_constraint_manager",
    srcs = ["linear_constraint_manager.cc"],
    hdrs = ["linear_constraint_manager.h"],
    deps = [
        ":integer",
        ":linear_constraint",
        ":model",
        ":sat_parameters_cc_proto",
        "//ortools/glop:revised_simplex",
        "//ortools/util:logging",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "cuts",
    srcs = ["cuts.cc"],
    hdrs = ["cuts.h"],
    deps = [
        ":diffn_util",
        ":implied_bounds",
        ":integer",
        ":intervals",
        ":linear_constraint",
        ":linear_constraint_manager",
        ":model",
        "//ortools/algorithms:knapsack_solver_for_cuts",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/util:time_limit",
    ],
)

cc_library(
    name = "scheduling_cuts",
    srcs = ["scheduling_cuts.cc"],
    hdrs = ["scheduling_cuts.h"],
    deps = [
        ":cuts",
        ":diffn_util",
        ":integer",
        ":intervals",
        ":linear_constraint",
        ":linear_constraint_manager",
        ":model",
        ":sat_base",
        ":util",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:stl_util",
        "//ortools/util:time_limit",
    ],
)

cc_library(
    name = "zero_half_cuts",
    srcs = ["zero_half_cuts.cc"],
    hdrs = ["zero_half_cuts.h"],
    deps = [
        ":integer",
        ":util",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/lp_data:base",
    ],
)

cc_library(
    name = "lp_utils",
    srcs = ["lp_utils.cc"],
    hdrs = ["lp_utils.h"],
    deps = [
        ":boolean_problem",
        ":boolean_problem_cc_proto",
        ":cp_model_cc_proto",
        ":cp_model_utils",
        ":integer",
        ":sat_base",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:strong_vector",
        "//ortools/glop:lp_solver",
        "//ortools/glop:parameters_cc_proto",
        "//ortools/linear_solver:linear_solver_cc_proto",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/util:fp_utils",
    ],
)

cc_library(
    name = "optimization",
    srcs = ["optimization.cc"],
    hdrs = ["optimization.h"],
    deps = [
        ":boolean_problem",
        ":boolean_problem_cc_proto",
        ":cp_model_mapping",
        ":encoding",
        ":integer",
        ":integer_expr",
        ":integer_search",
        ":model",
        ":pb_constraint",
        ":sat_base",
        ":sat_parameters_cc_proto",
        ":sat_solver",
        ":util",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/base:random",
        "//ortools/linear_solver:linear_solver_cc_proto",
        "//ortools/port:proto_utils",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    deps = [
        ":model",
        ":sat_base",
        ":sat_parameters_cc_proto",
        "//ortools/base",
        "//ortools/base:random",
        "//ortools/base:stl_util",
        "//ortools/util:random_engine",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/random:bit_gen_ref",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "table",
    srcs = ["table.cc"],
    hdrs = ["table.h"],
    deps = [
        ":integer",
        ":model",
        ":sat_base",
        ":sat_solver",
        ":util",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/base:stl_util",
        "//ortools/util:sorted_interval_list",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "cp_constraints",
    srcs = ["cp_constraints.cc"],
    hdrs = ["cp_constraints.h"],
    deps = [
        ":integer",
        ":model",
        ":sat_base",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/util:rev",
        "//ortools/util:sort",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "scheduling_constraints",
    srcs = ["scheduling_constraints.cc"],
    hdrs = ["scheduling_constraints.h"],
    deps = [
        ":integer",
        ":intervals",
        ":model",
        ":sat_base",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:int_type",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "diffn_util",
    srcs = ["diffn_util.cc"],
    hdrs = ["diffn_util.h"],
    deps = [
        ":integer",
        ":intervals",
        "//ortools/graph:connected_components",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "diffn",
    srcs = ["diffn.cc"],
    hdrs = ["diffn.h"],
    deps = [
        ":cumulative",
        ":diffn_util",
        ":disjunctive",
        ":integer",
        ":intervals",
        ":model",
        ":sat_base",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/util:rev",
        "//ortools/util:sort",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "circuit",
    srcs = ["circuit.cc"],
    hdrs = ["circuit.h"],
    deps = [
        ":integer",
        ":model",
        ":sat_base",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/util:rev",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "encoding",
    srcs = ["encoding.cc"],
    hdrs = ["encoding.h"],
    deps = [
        ":boolean_problem_cc_proto",
        ":pb_constraint",
        ":sat_base",
        ":sat_parameters_cc_proto",
        ":sat_solver",
        "//ortools/base",
        "//ortools/base:int_type",
    ],
)

cc_library(
    name = "cp_model_lns",
    srcs = ["cp_model_lns.cc"],
    hdrs = ["cp_model_lns.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_mapping",
        ":cp_model_utils",
        ":linear_programming_constraint",
        ":model",
        ":rins",
        ":subsolver",
        ":synchronization",
        "//ortools/base",
        "//ortools/base:threadpool",
        "//ortools/util:adaptative_parameter_value",
        "//ortools/util:random_engine",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "feasibility_pump",
    srcs = ["feasibility_pump.cc"],
    hdrs = ["feasibility_pump.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_mapping",
        ":integer",
        ":linear_constraint",
        ":synchronization",
        ":util",
        "//ortools/base",
        "//ortools/glop:revised_simplex",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_data_utils",
        "//ortools/util:saturated_arithmetic",
    ],
)

cc_library(
    name = "rins",
    srcs = ["rins.cc"],
    hdrs = ["rins.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cp_model_mapping",
        ":integer",
        ":linear_programming_constraint",
        ":model",
        ":synchronization",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "subsolver",
    srcs = ["subsolver.cc"],
    hdrs = ["subsolver.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ortools/base",
        "//ortools/base:threadpool",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "drat_proof_handler",
    srcs = ["drat_proof_handler.cc"],
    hdrs = ["drat_proof_handler.h"],
    deps = [
        ":drat_checker",
        ":drat_writer",
        ":sat_base",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:int_type",
        "//ortools/base:strong_vector",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "drat_checker",
    srcs = ["drat_checker.cc"],
    hdrs = ["drat_checker.h"],
    #    data = [
    #        "testdata/drup.cnf",
    #        "testdata/drup.drat",
    #    ],
    deps = [
        ":sat_base",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:int_type",
        "//ortools/base:stl_util",
        "//ortools/base:strong_vector",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "drat_writer",
    srcs = ["drat_writer.cc"],
    hdrs = ["drat_writer.h"],
    deps = [
        ":sat_base",
        "//ortools/base",
        "//ortools/base:file",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "cp_model_symmetries",
    srcs = ["cp_model_symmetries.cc"],
    hdrs = ["cp_model_symmetries.h"],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_utils",
        ":presolve_context",
        ":sat_parameters_cc_proto",
        ":symmetry_util",
        "//ortools/algorithms:find_graph_symmetries",
        "//ortools/algorithms:sparse_permutation",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/util:logging",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "swig_helper",
    hdrs = ["swig_helper.h"],
    visibility = [
        "//ortools/sat/java:__pkg__",
        "//ortools/sat/python:__pkg__",
    ],
    deps = [
        ":cp_model_cc_proto",
        ":cp_model_checker",
        ":cp_model_solver",
        ":model",
        ":sat_parameters_cc_proto",
        "//ortools/base",
        "@com_google_absl//absl/strings",
    ],
)
