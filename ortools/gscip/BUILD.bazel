load("@rules_cc//cc:defs.bzl", "cc_proto_library")

package(
    default_visibility = ["//visibility:public"],
)

proto_library(
    name = "gscip_proto",
    srcs = ["gscip.proto"],
)

cc_proto_library(
    name = "gscip_cc_proto",
    deps = [":gscip_proto"],
)

# NOTE(user): this file should ideally not have a compile time dependency on
# SCIP, so it can be used in client code.
cc_library(
    name = "gscip_parameters",
    srcs = ["gscip_parameters.cc"],
    hdrs = ["gscip_parameters.h"],
    deps = [
        ":gscip_cc_proto",
        "//ortools/base:status_macros",
        "@com_google_absl//absl/time",
    ],
)

#cc_test(
#    name = "gscip_parameters_test",
#    srcs = ["gscip_parameters_test.cc"],
#    deps = [
#        ":gscip_cc_proto",
#        ":gscip_parameters",
#        "@com_google_absl//absl/time",
#        "@com_google_googletest//:gtest_main",
#    ],
#)

cc_library(
    name = "legacy_scip_params",
    srcs = ["legacy_scip_params.cc"],
    hdrs = ["legacy_scip_params.h"],
    deps = [
        "//ortools/linear_solver:scip_helper_macros",
        "//ortools/linear_solver:scip_with_glop",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "gscip",
    srcs = [
        "gscip.cc",
    ],
    hdrs = ["gscip.h"],
    deps = [
        ":gscip_cc_proto",
        ":gscip_message_handler",
        ":gscip_parameters",
        ":legacy_scip_params",
        "//ortools/base",
        "//ortools/base:status_builder",
        "//ortools/base:status_macros",
        "//ortools/linear_solver:scip_helper_macros",
        "//ortools/linear_solver:scip_with_glop",
        "//ortools/port:proto_utils",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

#gmock not supported
#cc_library(
#    name = "gscip_testing",
#    testonly = 1,
#    srcs = ["gscip_testing.cc"],
#    hdrs = ["gscip_testing.h"],
#    deps = [
#        ":gscip",
#        ":gscip_parameters",
#        "@com_google_googletest//:gtest_main",
#    ],
#)

# Status matchers not supported.
# cc_test(
#    name = "gscip_test",
#    size = "small",
#    srcs = ["gscip_test.cc"],
#    deps = [
#        ":gscip",
#        ":gscip_cc_proto",
#        ":gscip_parameters",
#        ":gscip_testing",
#        "@com_google_googletest//:gtest",
#        "@com_google_googletest//:gtest_main",
#        "@scip//:libscip",
#    ],
#)

#cc_test(
#    name = "gscip_io_test",
#    size = "medium",
#    srcs = ["gscip_io_test.cc"],
#    deps = [
#        ":gscip",
#        ":gscip_cc_proto",
#        ":gscip_testing",
#        "//ortools/base/file",
#        "//file/base:path", # Path is not supported?
#        "@com_google_googletest//:gtest",
#        "@com_google_googletest//:gtest_main",
#        "@scip//:libscip",
#    ],
#)

cc_library(
    name = "gscip_ext",
    srcs = ["gscip_ext.cc"],
    hdrs = ["gscip_ext.h"],
    deps = [
        ":gscip",
        "//ortools/base:status_macros",
        "//ortools/linear_solver:scip_with_glop",
        "@com_google_absl//absl/status",
    ],
)

# Status matchers not supported.
#cc_test(
#    name = "gscip_ext_test",
#    size = "small",
#    srcs = ["gscip_ext_test.cc"],
#    deps = [
#        ":gscip",
#        ":gscip_cc_proto",
#        ":gscip_ext",
#        ":gscip_parameters",
#        ":gscip_testing",
#        "//ortools/base:map_util",
#        "@com_google_googletest//:gtest",
#        "@com_google_googletest//:gtest_main",
#        "@scip//:libscip",
#    ],
#)

cc_library(
    name = "gscip_event_handler",
    srcs = ["gscip_event_handler.cc"],
    hdrs = ["gscip_event_handler.h"],
    deps = [
        ":gscip",
        "//ortools/base",
        "//ortools/base:status_macros",
        "//ortools/linear_solver:scip_helper_macros",
        "//ortools/linear_solver:scip_with_glop",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "gscip_message_handler",
    srcs = ["gscip_message_handler.cc"],
    hdrs = ["gscip_message_handler.h"],
    deps = [
        "//ortools/base",
        "//ortools/linear_solver:scip_helper_macros",
        "//ortools/linear_solver:scip_with_glop",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)
