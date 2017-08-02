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
