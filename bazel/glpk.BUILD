cc_library(
    name = "glpk",
    srcs = glob(
        [
            "glpk-5.0/src/*.c",
            "glpk-5.0/src/*/*.c",
            "glpk-5.0/src/*.h",
            "glpk-5.0/src/*/*.h",
        ],
        exclude = [
            "glpk-5.0/src/proxy/main.c",
            "glpk-5.0/src/zlib/*",
        ],
    ),
    hdrs = [
        "glpk-5.0/src/glpk.h",
    ],
    copts = [
        "-w",
        "-Iexternal/glpk/glpk-5.0/src",
        "-Iexternal/glpk/glpk-5.0/src/amd",
        "-Iexternal/glpk/glpk-5.0/src/api",
        "-Iexternal/glpk/glpk-5.0/src/bflib",
        "-Iexternal/glpk/glpk-5.0/src/colamd",
        "-Iexternal/glpk/glpk-5.0/src/draft",
        "-Iexternal/glpk/glpk-5.0/src/env",
        "-Iexternal/glpk/glpk-5.0/src/intopt",
        "-Iexternal/glpk/glpk-5.0/src/minisat",
        "-Iexternal/glpk/glpk-5.0/src/misc",
        "-Iexternal/glpk/glpk-5.0/src/mpl",
        "-Iexternal/glpk/glpk-5.0/src/npp",
        "-Iexternal/glpk/glpk-5.0/src/proxy",
        "-Iexternal/glpk/glpk-5.0/src/simplex",
    ],
    deps = ["@zlib"],
    includes = ["glpk-5.0/src"],
    visibility = ["//visibility:public"],
)
