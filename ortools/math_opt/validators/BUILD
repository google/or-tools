package(default_visibility = ["//ortools/math_opt:__subpackages__"])

cc_library(
    name = "ids_validator",
    srcs = ["ids_validator.cc"],
    hdrs = ["ids_validator.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:status_macros",
        "//ortools/math_opt/core:model_summary",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "scalar_validator",
    srcs = ["scalar_validator.cc"],
    hdrs = ["scalar_validator.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "sparse_vector_validator",
    hdrs = ["sparse_vector_validator.h"],
    deps = [
        ":ids_validator",
        ":scalar_validator",
        "//ortools/base:status_macros",
        "//ortools/math_opt/core:sparse_vector_view",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "name_validator",
    srcs = ["name_validator.cc"],
    hdrs = ["name_validator.h"],
    deps = [
        ":sparse_vector_validator",
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/base:status_macros",
        "//ortools/math_opt/core:model_summary",
        "//ortools/math_opt/core:sparse_vector_view",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "model_validator",
    srcs = ["model_validator.cc"],
    hdrs = ["model_validator.h"],
    deps = [
        ":ids_validator",
        ":name_validator",
        ":scalar_validator",
        ":sparse_vector_validator",
        "//ortools/base",
        "//ortools/base:status_macros",
        "//ortools/math_opt:model_cc_proto",
        "//ortools/math_opt/core:model_summary",
        "//ortools/math_opt:model_update_cc_proto",
        "//ortools/math_opt:sparse_containers_cc_proto",
        "//ortools/math_opt/core:sparse_vector_view",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "solution_validator",
    srcs = ["solution_validator.cc"],
    hdrs = ["solution_validator.h"],
    deps = [
        ":ids_validator",
        ":scalar_validator",
        ":sparse_vector_validator",
        "//ortools/base",
        "//ortools/base:status_macros",
        "//ortools/math_opt/core:math_opt_proto_utils",
        "//ortools/math_opt:model_cc_proto",
        "//ortools/math_opt:model_parameters_cc_proto",
        "//ortools/math_opt/core:model_summary",
        "//ortools/math_opt:result_cc_proto",
        "//ortools/math_opt:solution_cc_proto",
        "//ortools/math_opt:sparse_containers_cc_proto",
        "//ortools/math_opt/core:sparse_vector_view",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "solver_parameters_validator",
    srcs = ["solver_parameters_validator.cc"],
    hdrs = ["solver_parameters_validator.h"],
    deps = [
        "//ortools/base:protoutil",
        "//ortools/base:status_macros",
        "//ortools/math_opt:parameters_cc_proto",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "callback_validator",
    srcs = ["callback_validator.cc"],
    hdrs = ["callback_validator.h"],
    deps = [
        ":model_parameters_validator",
        ":scalar_validator",
        ":solution_validator",
        ":sparse_vector_validator",
        "//ortools/base",
        "//ortools/base:status_macros",
        "//ortools/math_opt:callback_cc_proto",
        "//ortools/math_opt/core:model_summary",
        "//ortools/math_opt:solution_cc_proto",
        "//ortools/math_opt:sparse_containers_cc_proto",
        "//ortools/math_opt/core:sparse_vector_view",
        "//ortools/port:proto_utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "model_parameters_validator",
    srcs = ["model_parameters_validator.cc"],
    hdrs = ["model_parameters_validator.h"],
    deps = [
        ":ids_validator",
        "//ortools/base",
        "//ortools/base:status_macros",
        "//ortools/math_opt:model_parameters_cc_proto",
        "//ortools/math_opt/core:model_summary",
        "//ortools/math_opt:sparse_containers_cc_proto",
        "//ortools/math_opt/validators:solution_validator",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)
