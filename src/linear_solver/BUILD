package(default_visibility = ["//visibility:public"])

# If you want to use the GLPK solver, build with '--define USE_GLPK=' (or add
# it to your bazel.rc file). This will download, build and link to GLPK.
config_setting(
    name = "with_glpk",
    values = { "define": "USE_GLPK=" }
)

proto_library(
    name = "linear_solver_proto",
    srcs = ["linear_solver.proto"],
)

cc_proto_library(
    name = "linear_solver_cc_proto",
    deps = [":linear_solver_proto"],
)

cc_library(
    name = "linear_solver",
    srcs = [
        "bop_interface.cc",
        "glop_interface.cc",
        "glop_utils.cc",
        "linear_expr.cc",
        "linear_solver.cc",
        "model_exporter.cc",
        "model_validator.cc",
    ] +  select({
        ":with_glpk": ["glpk_interface.cc"],
        "//conditions:default": [],
    }),
    hdrs = [
        "glop_utils.h",
        "linear_expr.h",
        "linear_solver.h",
        "model_exporter.h",
        "model_validator.h",
    ],
    copts = [
        "-DUSE_GLOP",
        "-DUSE_BOP",
    ],
    defines = [
        "HAVE_CONFIG_H"
    ] +  select({
        ":with_glpk": ["USE_GLPK"],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
    deps = [
        ":linear_solver_cc_proto",
        "//base:timer",
        "//base",
        "//base:file",
        "//base:hash",
        "//base:map_util",
        "//base:stl_util",
        "//base:random",
        "//bop:bop_parameters_cc_proto",
        "//bop:integral_solver",
        "//glop",
        "//glop:parameters_cc_proto",
        "//lp_data",
        "//util:fp_utils",
        "//util:proto_tools",
    ] + select({
        ":with_glpk": ["@glpk//:glpk"],
        "//conditions:default": [],
    }),
)

