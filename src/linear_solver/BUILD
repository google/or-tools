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
    name = "linear_solver_glop",
    srcs = [
        "glop_interface.cc",
        "glop_utils.cc",
        "linear_solver.cc",
        "model_exporter.cc",
        "model_validator.cc",
    ],
    hdrs = [
        "glop_utils.h",
        "linear_solver.h",
        "model_exporter.h",
        "model_validator.h",
    ],
    copts = ["-DUSE_GLOP"],
    visibility = ["//visibility:public"],
    deps = [
        ":linear_solver_cc_proto",
        "//base",
        "//glop:glop",
        "//glop:parameters_cc_proto",
        "//lp_data",
        "//util",
    ],
)
