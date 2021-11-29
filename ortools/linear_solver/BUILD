load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@bazel_skylib//rules:copy_file.bzl", "copy_file")

package(default_visibility = ["//visibility:public"])

exports_files(["model_exporter_swig_helper.h"])

config_setting(
    name = "with_glpk",
    values = {"define": "USE_GLPK="},
)

config_setting(
    name = "with_cplex",
    values = {"define": "USE_CPLEX="},
)

config_setting(
    name = "with_xpress",
    values = {"define": "USE_XPRESS="},
)

config_setting(
    name = "with_cbc",
    values = {"define": "USE_CBC="},
)

config_setting(
    name = "with_clp",
    values = {"define": "USE_CLP="},
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

# You can include the interfaces to different solvers by invoking '--define'
# flags. By default GLOP, BOP, SCIP, GUROBI, and CP-SAT interface are included.
#
# For instance, if you want to use the GLPK solver, build with
# '--define USE_GLPK=' (or add it to your bazel.rc file). This will download,
# build and link to GLPK.
cc_library(
    name = "linear_solver",
    srcs = [
        "bop_interface.cc",
        "glop_interface.cc",
        "glop_utils.cc",
        "gurobi_interface.cc",
        "gurobi_proto_solver.cc",
        "linear_expr.cc",
        "linear_solver_callback.cc",
        "linear_solver.cc",
        "lpi_glop.cpp",
        "model_exporter.cc",
        "model_validator.cc",
        "sat_interface.cc",
        "sat_proto_solver.cc",
        "sat_solver_utils.cc",
        "scip_callback.cc",
        "scip_interface.cc",
        "scip_proto_solver.cc",
    ] + select({
        ":with_cbc": ["cbc_interface.cc"],
        "//conditions:default": [],
    }) + select({
        ":with_clp": ["clp_interface.cc"],
        "//conditions:default": [],
    }) + select({
        ":with_cplex": ["cplex_interface.cc"],
        "//conditions:default": [],
    }) + select({
        ":with_xpress": ["xpress_interface.cc"],
        "//conditions:default": [],
    }) + select({
        ":with_glpk": ["glpk_interface.cc"],
        "//conditions:default": [],
    }),
    hdrs = [
        "glop_interface.cc",
        "glop_utils.h",
        "gurobi_proto_solver.h",
        "linear_expr.h",
        "linear_solver.h",
        "linear_solver_callback.h",
        "model_exporter.h",
        "model_validator.h",
        "sat_proto_solver.h",
        "sat_solver_utils.h",
        "scip_callback.h",
        "scip_helper_macros.h",
        "scip_proto_solver.h",
    ],
    copts = ["-DUSE_SCIP"],
    deps = [
        ":linear_solver_cc_proto",
        ":scip_with_glop",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "//ortools/base:accurate_sum",
        "//ortools/base:dynamic_library",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/base:status_macros",
        "//ortools/base:stl_util",
        "//ortools/base:timer",
        "//ortools/base",
        "//ortools/bop:bop_parameters_cc_proto",
        "//ortools/bop:integral_solver",
        "//ortools/glop:lp_solver",
        "//ortools/glop:parameters_cc_proto",
        "//ortools/gscip:legacy_scip_params",
        "//ortools/gurobi:environment",
        "//ortools/port:file",
        "//ortools/port:proto_utils",
        "//ortools/sat:cp_model_cc_proto",
        "//ortools/sat:cp_model_solver",
        "//ortools/sat:lp_utils",
        "//ortools/util:fp_utils",
        "//ortools/util:lazy_mutable_copy",
    ] + select({
        ":with_glpk": ["@glpk//:glpk"],
        "//conditions:default": [],
    }),
)

copy_file(
    name = "lpi_glop",
    src = "@scip//:src/lpi/lpi_glop.cpp",
    out = "lpi_glop.cpp",
)

cc_library(
    name = "scip_with_glop",
    srcs = ["lpi_glop.cpp"],
    deps = [
        "//ortools/glop:lp_solver",
        "@scip//:libscip",
    ],
)

cc_library(
    name = "scip_helper_macros",
    hdrs = ["scip_helper_macros.h"],
    deps = [
        "//ortools/base:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ],
)
