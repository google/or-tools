load("@rules_cc//cc:defs.bzl", "cc_proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "jobshop_scheduling_proto",
    srcs = ["jobshop_scheduling.proto"],
    deps = [
        "@com_google_protobuf//:wrappers_proto",
    ],
)

cc_proto_library(
    name = "jobshop_scheduling_cc_proto",
    deps = [":jobshop_scheduling_proto"],
)

cc_library(
    name = "jobshop_scheduling_parser",
    srcs = ["jobshop_scheduling_parser.cc"],
    hdrs = ["jobshop_scheduling_parser.h"],
    deps = [
        ":jobshop_scheduling_cc_proto",
        "//ortools/base",
        "//ortools/base:filelineiter",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

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
        "@com_google_absl//absl/strings",
    ],
)
