cc_library(
    name = "glpk",
    srcs = glob([
        "glpk-4.65/src/*.c",
        "glpk-4.65/src/*/*.c",
        "glpk-4.65/src/*.h",
        "glpk-4.65/src/*/*.h",
    ], exclude = ["glpk-4.65/src/proxy/main.c"]),
    hdrs = [
        "glpk-4.65/src/glpk.h",
    ],
    copts = [
        "-Wno-error",
        "-w",
        "-Iexternal/glpk/glpk-4.65/src",
        "-Iexternal/glpk/glpk-4.65/src/amd",
        "-Iexternal/glpk/glpk-4.65/src/bflib",
        "-Iexternal/glpk/glpk-4.65/src/cglib",
        "-Iexternal/glpk/glpk-4.65/src/colamd",
        "-Iexternal/glpk/glpk-4.65/src/env",
        "-Iexternal/glpk/glpk-4.65/src/minisat",
        "-Iexternal/glpk/glpk-4.65/src/misc",
        "-Iexternal/glpk/glpk-4.65/src/proxy",
        "-Iexternal/glpk/glpk-4.65/src/zlib",
        "-DHAVE_ZLIB",
    ],
    includes=["glpk-4.65/src"],
    visibility = ["//visibility:public"],
)
