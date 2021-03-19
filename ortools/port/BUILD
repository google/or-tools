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
    srcs = ["sysinfo.cc"],
    hdrs = ["sysinfo.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:sysinfo",
    ],
)

cc_library(
    name = "file",
    srcs = [
        "file.cc",
    ],
    hdrs = [
        "file.h",
    ],
    deps = [
        "//ortools/base:file",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
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
        "@com_google_protobuf//:protobuf",
    ],
)
