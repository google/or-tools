package(default_visibility = ["//visibility:public"])

cc_proto_library(
    name = "parameters_cc_proto",
    deps = [":parameters_proto"],
)

proto_library(
    name = "parameters_proto",
    srcs = ["parameters.proto"],
)

SAFE_FP_CODE = ["-fno-fast-math"]

# GLOP library with minimal external dependencies for easier use on
# portable targets (e.g., Android, iOS).
# We define a separate target to:
#  * Avoid select(), which doesn't work with blaze query
#  * Define ANDROID_JNI, which is necessary for portable build
cc_library(
    name = "glop",
    srcs = [
        "lp_solver.cc",
        "preprocessor.cc",
    ],
    hdrs = [
        "lp_solver.h",
        "preprocessor.h",
    ],
    copts = SAFE_FP_CODE,
    deps = [
        ":parameters_cc_proto",
        ":revised_simplex",
        ":status",
        "//ortools/base",
        "//ortools/base:memory",
        "//ortools/base:strings",
        "//ortools/base:timer",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_data_utils",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:matrix_scaler",
        "//ortools/lp_data:matrix_utils",
        "//ortools/lp_data:proto_utils",
        "//ortools/util:file_util",
        "//ortools/util:fp_utils",
        "//ortools/util:time_limit",
    ],
)

cc_library(
    name = "basis_representation",
    srcs = ["basis_representation.cc"],
    hdrs = ["basis_representation.h"],
    deps = [
        ":lu_factorization",
        ":parameters_cc_proto",
        ":rank_one_update",
        ":status",
        "//ortools/base",
        "//ortools/base:stl_util",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:sparse",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "dual_edge_norms",
    srcs = ["dual_edge_norms.cc"],
    hdrs = ["dual_edge_norms.h"],
    deps = [
        ":basis_representation",
        ":parameters_cc_proto",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "entering_variable",
    srcs = ["entering_variable.cc"],
    hdrs = ["entering_variable.h"],
    deps = [
        ":basis_representation",
        ":parameters_cc_proto",
        ":primal_edge_norms",
        ":reduced_costs",
        ":status",
        ":update_row",
        ":variables_info",
        "//ortools/base:timer",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/port:proto_utils",
        "//ortools/util:bitset",
        "//ortools/util:random_engine",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "primal_edge_norms",
    srcs = ["primal_edge_norms.cc"],
    hdrs = ["primal_edge_norms.h"],
    deps = [
        ":basis_representation",
        ":parameters_cc_proto",
        ":update_row",
        ":variables_info",
        "//ortools/base:timer",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "reduced_costs",
    srcs = ["reduced_costs.cc"],
    hdrs = ["reduced_costs.h"],
    deps = [
        ":basis_representation",
        ":parameters_cc_proto",
        ":primal_edge_norms",
        ":status",
        ":update_row",
        ":variables_info",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/util:random_engine",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "status",
    srcs = ["status.cc"],
    hdrs = ["status.h"],
    deps = [
        "//ortools/base",
    ],
)

cc_library(
    name = "update_row",
    srcs = ["update_row.cc"],
    hdrs = ["update_row.h"],
    deps = [
        ":basis_representation",
        ":parameters_cc_proto",
        ":variables_info",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "variable_values",
    srcs = ["variable_values.cc"],
    hdrs = ["variable_values.h"],
    deps = [
        ":basis_representation",
        ":variables_info",
        "//ortools/graph:iterators",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "variables_info",
    srcs = ["variables_info.cc"],
    hdrs = ["variables_info.h"],
    deps = [
        "//ortools/lp_data:base",
        "//ortools/lp_data:sparse",
    ],
)

cc_library(
    name = "initial_basis",
    srcs = ["initial_basis.cc"],
    hdrs = ["initial_basis.h"],
    deps = [
        ":markowitz",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:sparse",
    ],
)

cc_library(
    name = "lu_factorization",
    srcs = ["lu_factorization.cc"],
    hdrs = ["lu_factorization.h"],
    deps = [
        ":markowitz",
        ":parameters_cc_proto",
        ":status",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:sparse",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "rank_one_update",
    hdrs = ["rank_one_update.h"],
    deps = [
        "//ortools/base",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:sparse",
    ],
)

cc_library(
    name = "markowitz",
    srcs = ["markowitz.cc"],
    hdrs = ["markowitz.h"],
    deps = [
        ":parameters_cc_proto",
        ":status",
        "//ortools/base",
        "//ortools/base:inlined_vector",
        "//ortools/base:strings",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:sparse",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "revised_simplex",
    srcs = ["revised_simplex.cc"],
    hdrs = ["revised_simplex.h"],
    deps = [
        ":basis_representation",
        ":dual_edge_norms",
        ":entering_variable",
        ":initial_basis",
        ":parameters_cc_proto",
        ":primal_edge_norms",
        ":reduced_costs",
        ":status",
        ":update_row",
        ":variable_values",
        "//ortools/base",
        "//ortools/base:strings",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_print_utils",
        "//ortools/lp_data:matrix_utils",
        "//ortools/lp_data:sparse_row",
        "//ortools/util:fp_utils",
        "//ortools/util:random_engine",
        "//ortools/util:time_limit",
    ],
)
