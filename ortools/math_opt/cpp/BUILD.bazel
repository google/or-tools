# External users should depend only on ":math_opt" and include
# "math_opt.h". Hence other libraries are private.
package(default_visibility = ["//ortools/math_opt:__subpackages__"])

cc_library(
    name = "math_opt",
    srcs = ["math_opt.cc"],
    hdrs = ["math_opt.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":callback",
        ":key_types",
        ":linear_constraint",
        ":model_solve_parameters",
        ":objective",
        ":result",
        ":variable_and_expressions",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:status_macros",
        "//ortools/math_opt:callback_cc_proto",
        "//ortools/math_opt/core:indexed_model",
        "//ortools/math_opt:model_cc_proto",
        "//ortools/math_opt:model_update_cc_proto",
        "//ortools/math_opt:parameters_cc_proto",
        "//ortools/math_opt:result_cc_proto",
        "//ortools/math_opt/core:solver",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "id_map",
    hdrs = ["id_map.h"],
    deps = [
        ":arrow_operator_proxy",
        ":key_types",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/math_opt/core:indexed_model",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "variable_and_expressions",
    srcs = ["variable_and_expressions.cc"],
    hdrs = ["variable_and_expressions.h"],
    deps = [
        ":id_map",
        ":key_types",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/math_opt/core:indexed_model",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "linear_constraint",
    srcs = ["linear_constraint.cc"],
    hdrs = ["linear_constraint.h"],
    deps = [
        ":id_map",
        ":key_types",
        ":variable_and_expressions",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/math_opt/core:indexed_model",
    ],
)

cc_library(
    name = "objective",
    srcs = ["objective.cc"],
    hdrs = ["objective.h"],
    deps = [
        ":key_types",
        ":variable_and_expressions",
        "//ortools/base",
        "//ortools/math_opt/core:indexed_model",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "result",
    srcs = ["result.cc"],
    hdrs = ["result.h"],
    deps = [
        ":linear_constraint",
        ":variable_and_expressions",
        "//ortools/base",
        "//ortools/base:protoutil",
        "//ortools/math_opt/core:indexed_model",
        "//ortools/math_opt:result_cc_proto",
        "//ortools/math_opt:solution_cc_proto",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "map_filter",
    hdrs = ["map_filter.h"],
    deps = [
        ":id_set",
        "//ortools/math_opt:sparse_containers_cc_proto",
        "//ortools/math_opt/core:indexed_model",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "callback",
    srcs = ["callback.cc"],
    hdrs = ["callback.h"],
    deps = [
        ":key_types",
        ":map_filter",
        ":variable_and_expressions",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:protoutil",
        "//ortools/base:status_macros",
        "//ortools/math_opt:callback_cc_proto",
        "//ortools/math_opt/core:indexed_model",
        "//ortools/math_opt:solution_cc_proto",
        "//ortools/math_opt:sparse_containers_cc_proto",
        "//ortools/math_opt/core:sparse_vector_view",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "arrow_operator_proxy",
    hdrs = ["arrow_operator_proxy.h"],
)

cc_library(
    name = "key_types",
    hdrs = ["key_types.h"],
    deps = [
        "//ortools/base",
        "//ortools/math_opt/core:indexed_model",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "id_set",
    hdrs = ["id_set.h"],
    deps = [
        ":arrow_operator_proxy",
        ":key_types",
        ":result",
        "//ortools/base",
        "//ortools/math_opt/core:indexed_model",
        "//ortools/math_opt:solution_cc_proto",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "model_solve_parameters",
    srcs = ["model_solve_parameters.cc"],
    hdrs = ["model_solve_parameters.h"],
    deps = [
        ":key_types",
        ":linear_constraint",
        ":map_filter",
        ":result",
        ":variable_and_expressions",
        "//ortools/math_opt/core:indexed_model",
        "//ortools/math_opt:model_parameters_cc_proto",
        "//ortools/math_opt:solution_cc_proto",
        "//ortools/math_opt:sparse_containers_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
