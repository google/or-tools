load("@rules_cc//cc:defs.bzl", "cc_proto_library")

package(
    default_visibility = ["//visibility:public"],
)

# Floating-point code in this directory must not be compiled with
# dangerous optimizations. For example do not assume that FP expressions
# are associative. This is what -fno-fast-math is for.
SAFE_FP_CODE = ["-fno-fast-math"]

# Revised Simplex LP solver.
cc_library(
    name = "pricing",
    hdrs = ["pricing.h"],
    deps = [
        "//ortools/base",
        "//ortools/lp_data:base",
        "//ortools/util:bitset",
        "//ortools/util:stats",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/random:bit_gen_ref",
    ],
)

cc_library(
    name = "revised_simplex",
    srcs = ["revised_simplex.cc"],
    hdrs = ["revised_simplex.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":basis_representation",
        ":dual_edge_norms",
        ":entering_variable",
        ":initial_basis",
        ":parameters_cc_proto",
        ":pricing",
        ":primal_edge_norms",
        ":reduced_costs",
        ":status",
        ":update_row",
        ":variable_values",
        ":variables_info",
        "//ortools/base",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_print_utils",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:matrix_utils",
        "//ortools/lp_data:scattered_vector",
        "//ortools/lp_data:sparse_row",
        "//ortools/util:fp_utils",
        "//ortools/util:random_engine",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

# Update row.

cc_library(
    name = "update_row",
    srcs = ["update_row.cc"],
    hdrs = ["update_row.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":basis_representation",
        ":parameters_cc_proto",
        ":variables_info",
        "//ortools/base",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:scattered_vector",
        "//ortools/util:stats",
    ],
)

# Variables info.

cc_library(
    name = "variables_info",
    srcs = ["variables_info.cc"],
    hdrs = ["variables_info.h"],
    copts = SAFE_FP_CODE,
    deps = [
        "//ortools/base",
        "//ortools/lp_data:base",
        "//ortools/lp_data:sparse",
    ],
)

# LU factorization of sparse matrices.

cc_library(
    name = "lu_factorization",
    srcs = ["lu_factorization.cc"],
    hdrs = ["lu_factorization.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":markowitz",
        ":parameters_cc_proto",
        ":status",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:scattered_vector",
        "//ortools/lp_data:sparse",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "markowitz",
    srcs = ["markowitz.cc"],
    hdrs = ["markowitz.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":parameters_cc_proto",
        ":status",
        "//ortools/base",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:sparse",
        "//ortools/util:stats",
        "@com_google_absl//absl/container:inlined_vector",
    ],
)

# Basis representations (Eta and LU).

cc_library(
    name = "basis_representation",
    srcs = ["basis_representation.cc"],
    hdrs = ["basis_representation.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":lu_factorization",
        ":parameters_cc_proto",
        ":rank_one_update",
        ":status",
        "//ortools/base",
        "//ortools/base:stl_util",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:scattered_vector",
        "//ortools/lp_data:sparse",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "rank_one_update",
    hdrs = ["rank_one_update.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":status",
        "//ortools/base",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:scattered_vector",
        "//ortools/lp_data:sparse",
    ],
)

# Initial basis.

cc_library(
    name = "initial_basis",
    srcs = ["initial_basis.cc"],
    hdrs = ["initial_basis.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":markowitz",
        "//ortools/base",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:sparse",
    ],
)

# Function status.

cc_library(
    name = "status",
    srcs = ["status.cc"],
    hdrs = ["status.h"],
    copts = SAFE_FP_CODE,
    deps = [
        "//ortools/base",
    ],
)

# Glop parameters.

cc_proto_library(
    name = "parameters_cc_proto",
    deps = [":parameters_proto"],
)

proto_library(
    name = "parameters_proto",
    srcs = ["parameters.proto"],
)

# Dual edge norms.

cc_library(
    name = "dual_edge_norms",
    srcs = ["dual_edge_norms.cc"],
    hdrs = ["dual_edge_norms.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":basis_representation",
        ":parameters_cc_proto",
        "//ortools/base",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:scattered_vector",
        "//ortools/util:stats",
    ],
)

# Primal edge norms.

cc_library(
    name = "primal_edge_norms",
    srcs = ["primal_edge_norms.cc"],
    hdrs = ["primal_edge_norms.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":basis_representation",
        ":parameters_cc_proto",
        ":update_row",
        ":variables_info",
        "//ortools/base",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:scattered_vector",
        "//ortools/util:stats",
    ],
)

cc_library(
    name = "reduced_costs",
    srcs = ["reduced_costs.cc"],
    hdrs = ["reduced_costs.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":basis_representation",
        ":parameters_cc_proto",
        ":pricing",
        ":primal_edge_norms",
        ":status",
        ":update_row",
        ":variables_info",
        "//ortools/base",
        "//ortools/base:random",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:scattered_vector",
        "//ortools/util:random_engine",
        "//ortools/util:stats",
        "@com_google_absl//absl/random:bit_gen_ref",
    ],
)

cc_library(
    name = "variable_values",
    srcs = ["variable_values.cc"],
    hdrs = ["variable_values.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":basis_representation",
        ":dual_edge_norms",
        ":pricing",
        ":variables_info",
        "//ortools/base",
        "//ortools/graph:iterators",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:scattered_vector",
        "//ortools/util:stats",
    ],
)

# Primal pricing rules.

cc_library(
    name = "entering_variable",
    srcs = ["entering_variable.cc"],
    hdrs = ["entering_variable.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":basis_representation",
        ":parameters_cc_proto",
        ":primal_edge_norms",
        ":reduced_costs",
        ":status",
        ":update_row",
        ":variables_info",
        "//ortools/base",
        "//ortools/base:random",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/port:proto_utils",
        "//ortools/util:bitset",
        "//ortools/util:random_engine",
        "//ortools/util:stats",
        "@com_google_absl//absl/random:bit_gen_ref",
    ],
)

# Preprocessor.

cc_library(
    name = "preprocessor",
    srcs = ["preprocessor.cc"],
    hdrs = ["preprocessor.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":parameters_cc_proto",
        ":revised_simplex",
        ":status",
        "//ortools/base",
        "//ortools/base:iterator_adaptors",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_data_utils",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:matrix_scaler",
        "//ortools/lp_data:matrix_utils",
        "@com_google_absl//absl/strings:str_format",
    ],
)

# LP Solver.

cc_library(
    name = "lp_solver",
    srcs = ["lp_solver.cc"],
    hdrs = ["lp_solver.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":parameters_cc_proto",
        ":preprocessor",
        ":status",
        "//ortools/base",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/lp_data:lp_utils",
        "//ortools/lp_data:proto_utils",
        "//ortools/util:file_util",
        "//ortools/util:fp_utils",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)
