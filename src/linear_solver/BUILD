package(default_visibility = ["//visibility:public"])

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
    ],
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
    visibility = ["//visibility:public"],
    deps = [
        ":linear_solver_cc_proto",
        "//base",
        "//base:file",
        "//base:hash",
        "//base:map_util",
        "//base:random",
        "//bop:bop_parameters_cc_proto",
        "//bop:integral_solver",
        "//glop",
        "//glop:parameters_cc_proto",
        "//lp_data",
        "//util:fp_utils",
        "//util:proto_tools",
    ],
)
