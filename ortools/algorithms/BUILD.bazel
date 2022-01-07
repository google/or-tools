package(default_visibility = ["//visibility:public"])

cc_library(
    name = "hungarian",
    srcs = ["hungarian.cc"],
    hdrs = ["hungarian.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:hash",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "hungarian_test",
    size = "medium",
    srcs = ["hungarian_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":hungarian",
        "//ortools/base",
        "//ortools/base:map_util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "knapsack_solver_base_lib",
    hdrs = ["knapsack_solver.h"],
    deps = [
        ":knapsack_solver_for_interface_lib",
        "//ortools/base",
        "//ortools/util:time_limit",
    ],
)

cc_library(
    name = "knapsack_solver_lib",
    hdrs = ["knapsack_solver.h"],
    deps = [
        ":knapsack_solver_for_interface_lib",
        "//ortools/base",
        "//ortools/linear_solver",
        "//ortools/util:time_limit",
    ],
)

cc_library(
    name = "knapsack_solver_for_interface_lib",
    srcs = ["knapsack_solver.cc"],
    hdrs = ["knapsack_solver.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:stl_util",
        "//ortools/linear_solver",
        "//ortools/util:bitset",
        "//ortools/util:time_limit",
    ],
)

cc_library(
    name = "knapsack_solver_for_cuts",
    srcs = ["knapsack_solver_for_cuts.cc"],
    hdrs = ["knapsack_solver_for_cuts.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ortools/base",
        "//ortools/base:int_type",
        "//ortools/base:ptr_util",
        "//ortools/base:strong_vector",
        "//ortools/util:time_limit",
    ],
)

cc_library(
    name = "dense_doubly_linked_list",
    hdrs = ["dense_doubly_linked_list.h"],
    deps = [
        "//ortools/base",
    ],
)

cc_library(
    name = "dynamic_partition",
    srcs = ["dynamic_partition.cc"],
    hdrs = ["dynamic_partition.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:murmur",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "sparse_permutation",
    srcs = ["sparse_permutation.cc"],
    hdrs = ["sparse_permutation.h"],
    deps = [
        "//ortools/base",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "dynamic_permutation",
    srcs = ["dynamic_permutation.cc"],
    hdrs = ["dynamic_permutation.h"],
    deps = [
        ":sparse_permutation",
        "//ortools/base",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "find_graph_symmetries",
    srcs = ["find_graph_symmetries.cc"],
    hdrs = ["find_graph_symmetries.h"],
    deps = [
        ":dense_doubly_linked_list",
        ":dynamic_partition",
        ":dynamic_permutation",
        ":sparse_permutation",
        "//ortools/base",
        "//ortools/graph",
        "//ortools/graph:iterators",
        "//ortools/graph:util",
        "//ortools/util:stats",
        "//ortools/util:time_limit",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)
