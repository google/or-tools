proto_library(
    name = "rcpsp_proto",
    srcs = ["rcpsp.proto"],
)

cc_proto_library(
    name = "rcpsp_cc_proto",
    deps = [":rcpsp_proto"],
)

cc_library(
    name = "rcpsp_parser",
    srcs = ["rcpsp_parser.cc"],
    hdrs = ["rcpsp_parser.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":rcpsp_cc_proto",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:filelineiter",
        "//ortools/base:strings",
    ],
)
