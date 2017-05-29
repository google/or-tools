package(default_visibility = ["//visibility:public"])

cc_library(
    name = "affine_relation",
    hdrs = ["affine_relation.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:iterator_adaptors",
    ],
)

cc_library(
    name = "filelineiter",
    hdrs = ["filelineiter.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:status",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "vector_map",
    hdrs = ["vector_map.h"],
    deps = ["//ortools/base:map_util"],
)

cc_library(
    name = "bitset",
    srcs = ["bitset.cc"],
    hdrs = ["bitset.h"],
    deps = ["//ortools/base"],
)

cc_library(
    name = "cached_log",
    srcs = ["cached_log.cc"],
    hdrs = ["cached_log.h"],
    deps = [
        "//ortools/base",
    ],
)

cc_library(
    name = "graph_export",
    srcs = ["graph_export.cc"],
    hdrs = ["graph_export.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:status",
    ],
)

cc_library(
    name = "iterators",
    hdrs = ["iterators.h"],
)

cc_library(
    name = "zvector",
    hdrs = ["zvector.h"],
    deps = ["//ortools/base"],
)

cc_library(
    name = "permutation",
    hdrs = ["permutation.h"],
    deps = ["//ortools/base"],
)

cc_library(
    name = "xml_helper",
    srcs = ["xml_helper.cc"],
    hdrs = ["xml_helper.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:strings",
    ],
)

#cc_library(
#    name = "step_function",
#    srcs = ["step_function.cc"],
#    hdrs = ["step_function.h"],
#    deps = [
#        "//ortools/base:strings",
#        ":iterators",
#        "//ortools/base",
#    ],
#)

cc_library(
    name = "saturated_arithmetic",
    hdrs = ["saturated_arithmetic.h"],
    deps = [
        ":bitset",
        "//ortools/base",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "piecewise_linear_function",
    srcs = ["piecewise_linear_function.cc"],
    hdrs = ["piecewise_linear_function.h"],
    deps = [
        ":saturated_arithmetic",
        "//ortools/base",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "rational_approximation",
    srcs = ["rational_approximation.cc"],
    hdrs = ["rational_approximation.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "sorted_interval_list",
    srcs = ["sorted_interval_list.cc"],
    hdrs = ["sorted_interval_list.h"],
    deps = [
        ":saturated_arithmetic",
        "//ortools/base",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "string_array",
    hdrs = ["string_array.h"],
)

cc_library(
    name = "tuple_set",
    hdrs = ["tuple_set.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:map_util",
    ],
)

cc_library(
    name = "stats",
    srcs = ["stats.cc"],
    hdrs = ["stats.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:stl_util",
        "//ortools/base:strings",
        #        "//ortools/base:strings:human_readable",
        "//ortools/base:sysinfo",
        "//ortools/base:timer",
        "//ortools/base:encodingutils",  # To replace by port when we export them.
    ],
)

cc_library(
    name = "time_limit",
    srcs = ["time_limit.cc"],
    hdrs = ["time_limit.h"],
    deps = [
        ":running_stat",
        "//ortools/base",
        "//ortools/base:strings",
        "//ortools/base:sysinfo",
        "//ortools/base:timer",
    ],
)

cc_library(
    name = "fp_utils",
    srcs = ["fp_utils.cc"],
    hdrs = ["fp_utils.h"],
    # -frounding-math is needed for code that uses IEEE-754 rounding modes.
    # You must also set this flag if you depend on this target and use
    # its methods related to IEEE-754 rounding modes.
    copts = ["-frounding-math"],
    deps = [
        ":bitset",
        "//ortools/base",
    ],
)

cc_library(
    name = "monoid_operation_tree",
    srcs = [],
    hdrs = ["monoid_operation_tree.h"],
    deps = ["//ortools/base"],
)

cc_library(
    name = "return_macros",
    hdrs = ["return_macros.h"],
    deps = ["//ortools/base"],
)

cc_library(
    name = "running_stat",
    hdrs = ["running_stat.h"],
    deps = ["//ortools/base"],
)

cc_library(
    name = "proto_tools",
    srcs = ["proto_tools.cc"],
    hdrs = ["proto_tools.h"],
    deps = [
        "//ortools/base:strings",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:hash",
        #        "//net/proto2/io/public",
        #        "//net/proto2/io/public:io",
        #        "//net/proto2/public",
        #        "//net/proto2/util/public:json",
        "@com_google_protobuf_cc//:protobuf",
    ],
)

cc_library(
    name = "functions_swig_helpers",
    hdrs = [
        "functions_swig_helpers.h",
    ],
    deps = ["//ortools/base"],
)

cc_library(
    name = "functions_swig_test_helpers",
    testonly = 1,
    hdrs = [
        "functions_swig_test_helpers.h",
    ],
    deps = ["//ortools/base"],
)

cc_library(
    name = "range_minimum_query",
    hdrs = ["range_minimum_query.h"],
    deps = [":bitset"],
)

cc_library(
    name = "range_query_function",
    srcs = ["range_query_function.cc"],
    hdrs = ["range_query_function.h"],
    deps = [
        ":range_minimum_query",
        "//ortools/base",
    ],
)

cc_library(
    name = "rev",
    hdrs = ["rev.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "vector_or_function",
    hdrs = ["vector_or_function.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ortools/base",
    ],
)

#cc_library(
#    name = "tsplib_parser",
#    srcs = ["tsplib_parser.cc"],
#    hdrs = ["tsplib_parser.h"],
#    visibility = ["//visibility:public"],
#    deps = [
#        "//ortools/base:strings",
#        ":filelineiter",
#        "//ortools/base",
#        "//ortools/base:file",
#        "//ortools/base:file:path",
#        "//ortools/base:map_util",
#        "//ortools/base:mathutil",
#        "///ortools/base:strings",
#        "//file/zipfile",
#        "//util/regexp/re2",
#    ],
#)

#cc_library(
#    name = "pdtsp_parser",
#    srcs = ["pdtsp_parser.cc"],
#    hdrs = ["pdtsp_parser.h"],
#    visibility = ["//visibility:public"],
#    deps = [
#        "//ortools/base:strings",
#        ":filelineiter",
#        "//ortools/base",
#        "//ortools/base:file",
#        "//ortools/base:file:path",
#        "//ortools/base:mathutil",
#    ],
#)

#cc_library(
#    name = "bp_parser",
#    srcs = ["bp_parser.cc"],
#    hdrs = ["bp_parser.h"],
#    visibility = ["//visibility:public"],
#    deps = [
#        "//ortools/base:strings",
#        ":filelineiter",
#        "//ortools/base",
#        "//ortools/base:file",
#    ],
#)

cc_library(
    name = "rcpsp_parser",
    srcs = ["rcpsp_parser.cc"],
    hdrs = ["rcpsp_parser.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":filelineiter",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "sort",
    hdrs = ["sort.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ortools/base",
    ],
)
