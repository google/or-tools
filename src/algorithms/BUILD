package(default_visibility = ["//visibility:public"])

cc_library(
    name = "dynamic_partition",
    srcs = ["dynamic_partition.cc"],
    hdrs = ["dynamic_partition.h"],
    deps = [
        "//base",
    ],
)

cc_library(
    name = "sparse_permutation",
    srcs = ["sparse_permutation.cc"],
    hdrs = ["sparse_permutation.h"],
    deps = [
        "//base",
    ],
)

cc_library(
    name = "dynamic_permutation",
    srcs = ["dynamic_permutation.cc"],
    hdrs = ["dynamic_permutation.h"],
    deps = [
        ":sparse_permutation",
        "//base",
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
    name = "find_graph_symmetries",
    srcs = ["find_graph_symmetries.cc"],
    hdrs = ["find_graph_symmetries.h"],
    deps = [
        ":dense_doubly_linked_list",
        ":dynamic_partition",
        ":dynamic_permutation",
        ":sparse_permutation",
        "//base",
        "//graph",
        "//graph:util",
        "//util",
    ],
)