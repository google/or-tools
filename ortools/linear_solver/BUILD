package(default_visibility = ["//visibility:public"])

# If you want to use the GLPK solver, build with '--define USE_GLPK=' (or add
# it to your bazel.rc file). This will download, build and link to GLPK.
config_setting(
    name = "with_glpk",
    values = {"define": "USE_GLPK="},
)

proto_library(
    name = "linear_solver_proto",
    srcs = ["linear_solver.proto"],
    deps = ["//ortools/util:optional_boolean_proto"],
)

cc_proto_library(
    name = "linear_solver_cc_proto",
    deps = [":linear_solver_proto"],
)

cc_library(
    name = "linear_solver",
    srcs = [
        "linear_expr.cc",
        "linear_solver.cc",
        "model_exporter.cc",
        "model_validator.cc",

        # By default include GLOP interface.
        "glop_interface.cc",
        "glop_utils.cc",
    ],
    hdrs = [
        "linear_expr.h",
        "linear_solver.h",
        "model_exporter.h",
        "model_validator.h",

        # By default include GLOP interface.
        "glop_interface.cc",
        "glop_utils.h",
    ],
    copts = [
        "-DUSE_GLOP",
    ],
    deps = [
        ":linear_solver_cc_proto",
        "//ortools/base",
        "//ortools/base:accurate_sum",
        "//ortools/base:canonical_errors",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/base:mutex",
        "//ortools/base:optional",
        "//ortools/base:status",
        "//ortools/base:stl_util",
        "//ortools/base:strings",
        "//ortools/base:timer",
        "//ortools/glop",
        "//ortools/glop:parameters_cc_proto",
        "//ortools/port:file",
        "//ortools/port:proto_utils",
        "//ortools/util:fp_utils",
    ],
)

cc_library(
    name = "interfaces",
    srcs = [
        "bop_interface.cc",
        "cbc_interface.cc",
        "clp_interface.cc",
        "cplex_interface.cc",
        "gurobi_interface.cc",
        "glop_interface.cc",
        "glop_utils.cc",
    ] + select({
        ":with_glpk": ["glpk_interface.cc"],
        "//conditions:default": [],
    }),
    hdrs = [
        "glop_utils.h",
    ],
    copts = [
        "-DUSE_GLOP",
        "-DUSE_BOP",
    ],
    defines = [
        "HAVE_CONFIG_H",
    ] + select({
        ":with_glpk": ["USE_GLPK"],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
    deps = [
        ":linear_solver",
        "//ortools/base",
        "//ortools/base:file",
        "//ortools/base:hash",
        "//ortools/base:strings",
        "//ortools/bop:bop_parameters_cc_proto",
        "//ortools/bop:integral_solver",
        "//ortools/glop",
        "//ortools/glop:parameters_cc_proto",
        "//ortools/lp_data",
        "//ortools/lp_data:base",
        "//ortools/port:proto_utils",
        "//ortools/util:time_limit",
        "@com_google_protobuf//:protobuf",
    ] + select({
        ":with_glpk": ["@glpk//:glpk"],
        "//conditions:default": [],
    }),
)
