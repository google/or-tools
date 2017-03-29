package(default_visibility = ["//visibility:public"])

#W_FLOAT_CONVERSION = select({
#    "//tools/cc_target_os:android": [""],
#    "//tools/cc_target_os:ios": ["-Xclang-only=-Wfloat-conversion"],
#    "//conditions:default": ["-Xclang-only=-Wfloat-conversion"],
#})

#SAFE_FP_CODE = W_FLOAT_CONVERSION + ["-fno-fast-math"]
SAFE_FP_CODE = ["-fno-fast-math"]

cc_library(
    name = "base",
    srcs = ["lp_types.cc"],
    hdrs = ["lp_types.h"],
    deps = [
        "//base",
        "//base:hash",
        "//base:int_type",
        "//base:int_type_indexed_vector",
        "//util:bitset",
    ],
)

cc_library(
    name = "permutation",
    hdrs = ["permutation.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        "//base",
        "//util:return_macros",
    ],
)

cc_library(
    name = "sparse_vector",
    hdrs = ["sparse_vector.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":permutation",
        "//base",
        "//base:strings",
        "//util:iterators",
        "//util:return_macros",
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

cc_library(
    name = "sparse_row",
    hdrs = ["sparse_row.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":sparse_vector",
        "//base",
    ],
)

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
        "//base:hash",
        "//base:int_type",
        "//base:int_type_indexed_vector",
        "//base:strings",
        "//util:fp_utils",
        "//util:return_macros",
    ],
)

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
        "//base:hash",
        "//base:int_type_indexed_vector",
        "//util:fp_utils",
    ],
)

cc_library(
    name = "matrix_scaler_hdr",
    hdrs = ["matrix_scaler.h"],
    deps = [
        ":base",
        "//base",
        "//base:int_type_indexed_vector",
    ],
)

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
        "//base:hash",
        "//base:int_type",
        "//base:int_type_indexed_vector",
        "//base:strings",
        "//util:fp_utils",
    ],
)

cc_library(
    name = "lp_utils",
    srcs = ["lp_utils.cc"],
    hdrs = ["lp_utils.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":sparse_column",
        "//base",
        "//base:accurate_sum",
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
        "//base:hash",
    ],
)

#cc_library(
#    name = "lp_parser",
#    testonly = 1,
#    srcs = ["lp_parser.cc"],
#    hdrs = ["lp_parser.h"],
#    copts = SAFE_FP_CODE,
#    deps = [
#        ":base",
#        ":lp_data",
#        "//base",
#        "//base:strings",
#        "//util/regexp/re2",
#    ],
#)

#cc_library(
#    name = "lp_constraint_classifier",
#    srcs = ["lp_constraint_classifier.cc"],
#    hdrs = ["lp_constraint_classifier.h"],
#    copts = SAFE_FP_CODE,
#    deps = [
#        ":lp_data",
#        "//util:fp_utils",
#    ],
#)

cc_library(
    name = "lp_print_utils",
    srcs = ["lp_print_utils.cc"],
    hdrs = ["lp_print_utils.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        "//base",
        "//base:strings",
        "//util:rational_approximation",
    ],
)

cc_library(
    name = "proto_utils",
    srcs = ["proto_utils.cc"],
    hdrs = ["proto_utils.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":lp_data",
        "//base",
        "//linear_solver:linear_solver_cc_proto",
    ],
)

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
        "//base:file",
        #        "//base:file:path",
        "//base:filelinereader",
        "//base:hash",
        "//base:int_type",
        "//base:int_type_indexed_vector",
        "//base:map_util",
        "//base:status",
        "//base:strings",
        #        "//file/localfile",
        #        "//file/memfile",
        #        "//file/zipfile",
    ],
)

cc_library(
    name = "lp_decomposer",
    srcs = ["lp_decomposer.cc"],
    hdrs = ["lp_decomposer.h"],
    copts = SAFE_FP_CODE,
    deps = [
        ":base",
        ":lp_data",
        ":lp_utils",
        "//algorithms:dynamic_partition",
        "//base",
        "//base:hash",
        "//base:mutex",
    ],
)
