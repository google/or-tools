package(default_visibility = ["//ortools/math_opt:__subpackages__"])

cc_library(
    name = "math_opt_proto_utils",
    srcs = ["math_opt_proto_utils.cc"],
    hdrs = ["math_opt_proto_utils.h"],
    deps = [
        ":sparse_vector_view",
        "//ortools/base",
        "//ortools/math_opt:callback_cc_proto",
        "//ortools/math_opt:model_cc_proto",
        "//ortools/math_opt:model_update_cc_proto",
        "//ortools/math_opt:sparse_containers_cc_proto",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "sparse_vector_view",
    hdrs = ["sparse_vector_view.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/math_opt:sparse_containers_cc_proto",
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
    name = "model_update_merge",
    srcs = ["model_update_merge.cc"],
    hdrs = ["model_update_merge.h"],
    deps = [
        "//ortools/base",
        "//ortools/math_opt:model_cc_proto",
        "//ortools/math_opt:model_update_cc_proto",
        "//ortools/math_opt:sparse_containers_cc_proto",
    ],
)

cc_library(
    name = "indexed_model",
    srcs = ["indexed_model.cc"],
    hdrs = ["indexed_model.h"],
    deps = [
        ":sparse_vector_view",
        ":model_update_merge",
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:map_util",
        "//ortools/math_opt:model_cc_proto",
        "//ortools/math_opt:model_update_cc_proto",
        "//ortools/math_opt:result_cc_proto",
        "//ortools/math_opt:solution_cc_proto",
        "//ortools/math_opt:sparse_containers_cc_proto",
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
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/math_opt:callback_cc_proto",
        "//ortools/math_opt:model_cc_proto",
        "//ortools/math_opt:model_parameters_cc_proto",
        "//ortools/math_opt:model_update_cc_proto",
        "//ortools/math_opt:parameters_cc_proto",
        "//ortools/math_opt:result_cc_proto",
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
        ":model_summary",
        ":solver_interface",
        "//ortools/base",
        "//ortools/base:status_macros",
        "//ortools/math_opt:callback_cc_proto",
        "//ortools/math_opt:model_cc_proto",
        "//ortools/math_opt:model_parameters_cc_proto",
        "//ortools/math_opt:model_update_cc_proto",
        "//ortools/math_opt:parameters_cc_proto",
        "//ortools/math_opt:result_cc_proto",
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
