cc_library(
    name = "main",
    srcs = glob(
        ["src/*.cc",
         "src/*.h"],
        exclude = ["src/gtest-all.cc"]
    ),
    hdrs = glob([
        "include/**/*.h",
        "**/*.h"]),
    copts = [
    	  "-Iexternal/gtest/include",
          "-Iexternal/gtest"
    ],
    linkopts = ["-pthread"],
    visibility = ["//visibility:public"],
)
