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
        "basis_representation.cc",
        "dual_edge_norms.cc",
        "entering_variable.cc",
        "initial_basis.cc",
        "lp_solver.cc",
        "lu_factorization.cc",
        "markowitz.cc",
        "preprocessor.cc",
        "primal_edge_norms.cc",
        "reduced_costs.cc",
        "revised_simplex.cc",
        "status.cc",
        "update_row.cc",
        "variable_values.cc",
        "variables_info.cc",
    ],
    hdrs = [
        "basis_representation.h",
        "dual_edge_norms.h",
        "entering_variable.h",
        "initial_basis.h",
        "lp_solver.h",
        "lu_factorization.h",
        "markowitz.h",
        "preprocessor.h",
        "primal_edge_norms.h",
        "rank_one_update.h",
        "reduced_costs.h",
        "revised_simplex.h",
        "status.h",
        "update_row.h",
        "variable_values.h",
        "variables_info.h",
    ],
    copts = SAFE_FP_CODE,
    visibility = ["//visibility:public"],
    deps = [
        ":parameters_cc_proto",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:random",
        "//ortools/base:stl_util",
        "//ortools/base:strings",
        "//ortools/linear_solver:linear_solver_cc_proto",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_print_utils",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:matrix_scaler",
        "//ortools/lp_data:matrix_utils",
        "//ortools/lp_data:proto_utils",
        "//ortools/lp_data:sparse",
        "//ortools/lp_data:sparse_row",
        "//ortools/util:bitset",
        "//ortools/util:file_util",
        "//ortools/util:fp_utils",
        "//ortools/util:iterators",
        "//ortools/util:random_engine",
        "//ortools/util:stats",
        "//ortools/util:time_limit",
    ],
)
