package(default_visibility = ["//ortools/math_opt:__subpackages__"])

cc_library(
    name = "proto_converter",
    srcs = ["proto_converter.cc"],
    hdrs = ["proto_converter.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ortools/math_opt/core:math_opt_proto_utils",
        "//ortools/math_opt:model_cc_proto",
        "//ortools/math_opt:sparse_containers_cc_proto",
        "//ortools/math_opt/core:sparse_vector_view",
        "//ortools/base",
        "//ortools/base:status_macros",
        "//ortools/linear_solver:linear_solver_cc_proto",
        # Only needed for linear_solver/model_validator.h, we should break that
        # target up.
        "//ortools/linear_solver",
        "//ortools/math_opt/validators:model_validator",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)
