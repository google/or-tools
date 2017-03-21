package(default_visibility = ["//visibility:public"])

# Enable a warning to check for floating point to integer conversions.
# In GCC-4.8, this was "-Wreal-conversion", but was removed in 4.9
# In Clang, this warning is "-Wfloat-conversion"
#W_FLOAT_CONVERSION = select({
#    "//tools/cc_target_os:android": [""],
#    "//tools/cc_target_os:ios": ["-Xclang-only=-Wfloat-conversion"],
#    "//conditions:default": ["-Xclang-only=-Wfloat-conversion"],
#})

# Floating-point code in this directory must not be compiled with
# dangerous optimizations. For example do not assume that FP expressions
# are associative. This is what -fno-fast-math is for.
SAFE_FP_CODE = ["-fno-fast-math"]

cc_library(
    name = "base",
    srcs = ["lp_types.cc"],
    hdrs = ["lp_types.h"],
    deps = [
        "//base",
        "//util",
    ],
)

# Handling of permutations.

cc_library(
    name = "permutation",
    hdrs = ["permutation.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        "//base",
        "//util",
    ],
)

# Compressed sparse columns.

cc_library(
    name = "sparse_vector",
    hdrs = ["sparse_vector.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":permutation",
        "//base",
        "//util",
    ],
)

cc_library(
    name = "sparse_column",
    srcs = ["sparse_column.cc"],
    hdrs = ["sparse_column.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":sparse_vector",
        "//base",
    ],
)

#cc_library(
#    name = "sparse_row",
#    hdrs = ["sparse_row.h"],
#    copts = SAFE_FP_CODE,
#    deps = [
#        ":base",
#        ":sparse_vector",
#        "//base",
#    ],
#)

# Column-compressed sparse matrices.

cc_library(
    name = "sparse",
    srcs = ["sparse.cc"],
    hdrs = [
        "sparse.h",
    ],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":matrix_scaler_hdr",
        ":permutation",
        ":sparse_column",
        "//base",
        "//util",
    ],
)

# Matrix scaler.

cc_library(
    name = "matrix_scaler",
    srcs = ["matrix_scaler.cc"],
    hdrs = ["matrix_scaler.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":lp_utils",
        ":sparse",
        "//base",
        "//util",
    ],
)

# This header-only library was inserted because the target
#   //lp_data:sparse,
# which includes matrix_scaler.h
# cannot depend on the library
#   :matrix_scaler: Found an inverse include path.
# See http://go/cxx-layering-faq
cc_library(
    name = "matrix_scaler_hdr",
    hdrs = ["matrix_scaler.h"],
    visibility = ["//visibility:private"],
    deps = [
        ":base",
        "//base",
    ],
)

# Linear Programming data storage.

cc_library(
    name = "lp_data",
    srcs = ["lp_data.cc"],
    hdrs = ["lp_data.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":lp_print_utils",
        ":lp_utils",
        ":matrix_scaler",
        ":matrix_utils",
        ":permutation",
        ":sparse",
        "//base",
        "//util",
    ],
)

# Lp utilities.

cc_library(
    name = "lp_utils",
    srcs = ["lp_utils.cc"],
    hdrs = ["lp_utils.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":sparse_column",
        "//base",
    ],
)

cc_library(
    name = "matrix_utils",
    srcs = ["matrix_utils.cc"],
    hdrs = ["matrix_utils.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":sparse",
        "//base",
    ],
)

# Linear Programming printing utilities.

cc_library(
    name = "lp_print_utils",
    srcs = ["lp_print_utils.cc"],
    hdrs = ["lp_print_utils.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        "//base",
        "//util",
    ],
)

# Proto conversion

cc_library(
    name = "proto_utils",
    srcs = ["proto_utils.cc"],
    hdrs = ["proto_utils.h"],
    copts = SAFE_FP_CODE,
    visibility = ["//visibility:public"],
    deps = [
        ":base",
        ":lp_data",
        "//base",
        "//linear_solver:linear_solver_cc_proto",
    ],
)

# MPS reader.

cc_library(
    name = "mps_reader",
    srcs = ["mps_reader.cc"],
    hdrs = ["mps_reader.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":lp_data",
        ":lp_print_utils",
        "//base",
    ],
)

# Decompose a LinearProgram into several independent LinearPrograms.

cc_library(
    name = "lp_decomposer",
    srcs = ["lp_decomposer.cc"],
    hdrs = ["lp_decomposer.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":lp_data",
        ":lp_utils",
        "//base",
        "//algorithms:dynamic_partition",
    ],
)