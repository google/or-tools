package(default_visibility = ["//visibility:public"])

cc_library(
    name = "hungarian",
    srcs = ["hungarian.cc"],
    hdrs = ["hungarian.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:hash",
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
        "//ortools/base:random",
        "//ortools/base:strings",
        "@gtest//:main",
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
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "sparse_permutation",
    srcs = ["sparse_permutation.cc"],
    hdrs = ["sparse_permutation.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "dynamic_permutation",
    srcs = ["dynamic_permutation.cc"],
    hdrs = ["dynamic_permutation.h"],
    deps = [
        ":sparse_permutation",
        "//ortools/base",
        "//ortools/base:strings",
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
        "//ortools/base:status",
        "//ortools/base:strings",
        "//ortools/graph",
        "//ortools/graph:util",
        "//ortools/util:iterators",
        "//ortools/util:stats",
        "//ortools/util:time_limit",
    ],
)
