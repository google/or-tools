package(default_visibility = ["//visibility:public"])

cc_library(
    name = "utf8",
    hdrs = ["utf8.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:encodingutils",
    ],
)

cc_library(
    name = "sysinfo",
    srcs = ["sysinfo_nonport.cc"],
    hdrs = ["sysinfo.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:sysinfo",
    ],
)

cc_library(
    name = "file",
    srcs = [
        "file_nonport.cc",
    ],
    hdrs = [
        "file.h",
    ],
    deps = [
        "//ortools/base:file",
        "//ortools/base:status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "proto_utils",
    hdrs = [
        "proto_utils.h",
    ],
    deps = [
        "//ortools/base",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf_cc//:protobuf",
    ],
)
