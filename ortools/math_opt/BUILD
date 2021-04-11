load("@rules_cc//cc:defs.bzl", "cc_proto_library")

package(default_visibility = ["//ortools/math_opt:__subpackages__"])

proto_library(
    name = "callback_proto",
    srcs = ["callback.proto"],
    deps = [
        ":solution_proto",
        ":sparse_containers_proto",
        "@com_google_protobuf//:duration_proto",
    ],
)

cc_proto_library(
    name = "callback_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":callback_proto",
    ],
)

proto_library(
    name = "model_proto",
    srcs = ["model.proto"],
    deps = [
        ":sparse_containers_proto",
    ],
)

cc_proto_library(
    name = "model_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":model_proto",
    ],
)

proto_library(
    name = "model_parameters_proto",
    srcs = ["model_parameters.proto"],
    deps = [
        ":solution_proto",
        ":sparse_containers_proto",
    ],
)

cc_proto_library(
    name = "model_parameters_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":model_parameters_proto",
    ],
)

proto_library(
    name = "model_update_proto",
    srcs = ["model_update.proto"],
    deps = [
        ":model_proto",
        ":sparse_containers_proto",
    ],
)

cc_proto_library(
    name = "model_update_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":model_update_proto",
    ],
)

proto_library(
    name = "parameters_proto",
    srcs = ["parameters.proto"],
    deps = [
        "//ortools/glop:parameters_proto",
        "//ortools/gscip:gscip_proto",
        "//ortools/sat:sat_parameters_proto",
        "@com_google_protobuf//:duration_proto",
    ],
)

cc_proto_library(
    name = "parameters_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":parameters_proto",
    ],
)

proto_library(
    name = "result_proto",
    srcs = ["result.proto"],
    deps = [
        ":solution_proto",
        "//ortools/gscip:gscip_proto",
        "@com_google_protobuf//:duration_proto",
    ],
)

cc_proto_library(
    name = "result_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":result_proto",
    ],
)

proto_library(
    name = "solution_proto",
    srcs = ["solution.proto"],
    deps = [
        ":sparse_containers_proto",
    ],
)

cc_proto_library(
    name = "solution_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":solution_proto",
    ],
)

proto_library(
    name = "sparse_containers_proto",
    srcs = ["sparse_containers.proto"],
)

cc_proto_library(
    name = "sparse_containers_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":sparse_containers_proto",
    ],
)

cc_library(
    name = "math_opt_proto_utils",
    srcs = ["math_opt_proto_utils.cc"],
    hdrs = ["math_opt_proto_utils.h"],
    deps = [
        ":callback_cc_proto",
        ":model_cc_proto",
        ":sparse_containers_cc_proto",
        ":sparse_vector_view",
        "//ortools/base",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "sparse_vector_view",
    hdrs = ["sparse_vector_view.h"],
    deps = [
        ":sparse_containers_cc_proto",
        "//ortools/base",
        "//ortools/base:map_util",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "model_summary",
    srcs = ["model_summary.cc"],
    hdrs = ["model_summary.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:linked_hash_map",
        "//ortools/base:map_util",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "proto_converter",
    srcs = ["proto_converter.cc"],
    hdrs = ["proto_converter.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":math_opt_proto_utils",
        ":model_cc_proto",
        ":sparse_containers_cc_proto",
        ":sparse_vector_view",
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

cc_library(
    name = "indexed_model",
    srcs = ["indexed_model.cc"],
    hdrs = ["indexed_model.h"],
    deps = [
        ":model_cc_proto",
        ":model_update_cc_proto",
        ":result_cc_proto",
        ":solution_cc_proto",
        ":sparse_containers_cc_proto",
        ":sparse_vector_view",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "solver_interface",
    srcs = ["solver_interface.cc"],
    hdrs = ["solver_interface.h"],
    deps = [
        ":callback_cc_proto",
        ":model_cc_proto",
        ":model_parameters_cc_proto",
        ":model_update_cc_proto",
        ":parameters_cc_proto",
        ":result_cc_proto",
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/port:proto_utils",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "solver",
    srcs = ["solver.cc"],
    hdrs = ["solver.h"],
    deps = [
        ":callback_cc_proto",
        ":model_cc_proto",
        ":model_parameters_cc_proto",
        ":model_summary",
        ":model_update_cc_proto",
        ":parameters_cc_proto",
        ":result_cc_proto",
        ":solver_interface",
        "//ortools/base",
        "//ortools/base:status_macros",
        "//ortools/math_opt/validators:callback_validator",
        "//ortools/math_opt/validators:model_parameters_validator",
        "//ortools/math_opt/validators:model_validator",
        "//ortools/math_opt/validators:solution_validator",
        "//ortools/math_opt/validators:solver_parameters_validator",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)
