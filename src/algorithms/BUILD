package(default_visibility = ["//visibility:public"])

cc_library(
    name = "hungarian",
    srcs = ["hungarian.cc"],
    hdrs = ["hungarian.h"],
    deps = [
        "//base",
        "//base:hash",
    ],
)

cc_library(
    name = "knapsack_solver_base_lib",
    hdrs = ["knapsack_solver.h"],
    deps = [
        ":knapsack_solver_for_interface_lib",
        "//base",
        "//util:time_limit",
    ],
)

cc_library(
    name = "knapsack_solver_lib",
    hdrs = ["knapsack_solver.h"],
    deps = [
        ":knapsack_solver_for_interface_lib",
        "//base",
        "//linear_solver:linear_solver_glop",
        #        "//linear_solver:linear_solver_cbc",
        #        "//linear_solver:linear_solver_glpk",
        #        "//linear_solver:linear_solver_scip",
        "//util:time_limit",
    ],
)

cc_library(
    name = "knapsack_solver_for_interface_lib",
    srcs = ["knapsack_solver.cc"],
    hdrs = ["knapsack_solver.h"],
    deps = [
        "//base",
        "//base:stl_util",
        # We don't link any underlying solver to let the linear_solver_knapsack
        # decide what solvers to include.
        "//linear_solver:linear_solver_glop",
        "//util:bitset",
        "//util:time_limit",
    ],
)

cc_library(
    name = "dense_doubly_linked_list",
    hdrs = ["dense_doubly_linked_list.h"],
    deps = [
        "//base",
    ],
)

cc_library(
    name = "dynamic_partition",
    srcs = ["dynamic_partition.cc"],
    hdrs = ["dynamic_partition.h"],
    deps = [
        "//base",
        "//base:murmur",
        "//base:strings",
    ],
)

cc_library(
    name = "sparse_permutation",
    srcs = ["sparse_permutation.cc"],
    hdrs = ["sparse_permutation.h"],
    deps = [
        "//base",
        "//base:strings",
    ],
)

cc_library(
    name = "dynamic_permutation",
    srcs = ["dynamic_permutation.cc"],
    hdrs = ["dynamic_permutation.h"],
    deps = [
        ":sparse_permutation",
        "//base",
        "//base:strings",
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
        "//base",
        "//base:status",
        "//base:strings",
        "//graph",
        "//graph:util",
        "//util:iterators",
        "//util:stats",
        "//util:time_limit",
    ],
)
