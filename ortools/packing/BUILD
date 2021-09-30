load("@rules_cc//cc:defs.bzl", "cc_proto_library")

package(default_visibility = ["//visibility:public"])

### Arc Flow ###

cc_library(
    name = "arc_flow_builder",
    srcs = ["arc_flow_builder.cc"],
    hdrs = ["arc_flow_builder.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/base:stl_util",
        "//ortools/graph:topologicalsorter",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "arc_flow_solver",
    srcs = [
        "arc_flow_solver.cc",
    ],
    hdrs = ["arc_flow_solver.h"],
    deps = [
        ":arc_flow_builder",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/linear_solver",
        "//ortools/packing:vector_bin_packing_cc_proto",
        "@com_google_absl//absl/flags:flag",
    ],
)

### Vector Bin Packing ###

proto_library(
    name = "vector_bin_packing_proto",
    srcs = ["vector_bin_packing.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "vector_bin_packing_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":vector_bin_packing_proto"],
)

cc_library(
    name = "vector_bin_packing_parser",
    srcs = ["vector_bin_packing_parser.cc"],
    hdrs = ["vector_bin_packing_parser.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":vector_bin_packing_cc_proto",
        "//ortools/base",
        "//ortools/base:filelineiter",
        "@com_google_absl//absl/strings",
    ],
)
