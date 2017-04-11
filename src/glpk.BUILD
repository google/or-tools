cc_library(
    name = "glpk",
    srcs = glob([
        "glpk-4.47/src/*.c",
        "glpk-4.47/src/*/*.c",
        "glpk-4.47/src/*.h",
        "glpk-4.47/src/*/*.h",
    ]),
    hdrs = [
        "glpk-4.47/src/glpk.h",
    ],
    copts = [
        "-Wno-error",
        "-w",
        "-Iexternal/glpk/glpk-4.47/src",
        "-DHAVE_ZLIB",
    ],
    includes=["glpk-4.47/src"],
    visibility = ["//visibility:public"],
)
