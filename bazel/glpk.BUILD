cc_library(
    name = "glpk",
    srcs = glob(
        [
            "glpk-5.0/src/*.c",
            "glpk-5.0/src/*/*.c",
            "glpk-5.0/src/*.h",
            "glpk-5.0/src/*/*.h",
        ],
        exclude = ["glpk-5.0/src/proxy/main.c"],
    ),
    hdrs = [
        "glpk-5.0/src/glpk.h",
    ],
    copts = [
        "-Wno-error",
        "-w",
        "-Iexternal/glpk/glpk-5.0/src",
        "-Iexternal/glpk/glpk-5.0/src/amd",
        "-Iexternal/glpk/glpk-5.0/src/bflib",
        "-Iexternal/glpk/glpk-5.0/src/cglib",
        "-Iexternal/glpk/glpk-5.0/src/colamd",
        "-Iexternal/glpk/glpk-5.0/src/env",
        "-Iexternal/glpk/glpk-5.0/src/minisat",
        "-Iexternal/glpk/glpk-5.0/src/misc",
        "-Iexternal/glpk/glpk-5.0/src/proxy",
        "-Iexternal/glpk/glpk-5.0/src/zlib",
        # "-DHAVE_ZLIB",
    ],
    includes = ["glpk-5.0/src"],
    visibility = ["//visibility:public"],
)
