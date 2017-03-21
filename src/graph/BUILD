package(default_visibility = ["//visibility:public"])

cc_library(
    name = "graph",
    hdrs = ["graph.h"],
    deps = [
        "//base",
        "//util",
    ],
)

cc_library(
    name = "graphs",
    hdrs = ["graphs.h"],
    deps = [
        ":ebert_graph",
        ":graph",
    ],
)

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    deps = [
        ":graph",
        "//base",
    ],
)

cc_library(
    name = "io",
    hdrs = ["io.h"],
    deps = [
        ":graph",
        "//base",
        "//util",
    ],
)

cc_library(
    name = "cliques",
    srcs = ["cliques.cc"],
    hdrs = ["cliques.h"],
    deps = [
        "//base",
        "//util",
    ],
)

cc_library(
    name = "hamiltonian_path",
    hdrs = ["hamiltonian_path.h"],
    deps = [
        "//base",
        "//util",
    ],
)

cc_library(
    name = "minimum_spanning_tree",
    hdrs = ["minimum_spanning_tree.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":connectivity",
        ":graph",
        "//base",
        "//util",
    ],
)

cc_library(
    name = "ebert_graph",
    hdrs = ["ebert_graph.h"],
    deps = [
        "//base",
        "//util",
    ],
)

cc_library(
    name = "connectivity",
    hdrs = ["connectivity.h"],
    deps = [
        "//base",
    ],
)