cc_library(
    name = "main",
    srcs = glob(
        ["googletest-release-1.8.0/googletest/src/*.cc",
         "googletest-release-1.8.0/googletest/src/*.h"],
        exclude = ["googletest-release-1.8.0/googletest/src/gtest-all.cc"]
    ),
    hdrs = glob([
        "googletest-release-1.8.0/googletest/include/**/*.h",
        "googletest-release-1.8.0/googletest/**/*.h"]),
    copts = ["-Iexternal/gtest/googletest-release-1.8.0/googletest/include",
             "-Iexternal/gtest/googletest-release-1.8.0/googletest"],
    linkopts = ["-pthread"],
    visibility = ["//visibility:public"],
)