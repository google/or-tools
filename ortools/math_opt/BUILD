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
