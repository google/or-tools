package(default_visibility = ["//visibility:public"])

cc_library(
    name = "graph",
    hdrs = ["graph.h"],
    deps = [
        "//ortools/base",
        "//ortools/util:iterators",
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
        "//ortools/base:hash",
        "//ortools/base:map_util",
    ],
)

cc_library(
    name = "io",
    hdrs = ["io.h"],
    deps = [
        ":graph",
        "//ortools/base:status",
        "//ortools/base:statusor",
        "//ortools/base:strings",
        "//ortools/util:filelineiter",
    ],
)

#cc_library(
#    name = "digraph",
#    srcs = ["digraph.cc"],
#    hdrs = ["digraph.h"],
#    deps = [
#        "//ortools/base",
#        "//ortools/base:int_type",
#        "//ortools/base:int_type_indexed_vector",
#    ],
#)

#proto_library(
#    name = "paths_proto",
#    srcs = ["paths.proto"],
#)

#cc_proto_library(
#    name = "paths_cc_proto",
#    deps = [":paths_proto"],
#)

cc_library(
    name = "shortestpaths",
    srcs = [
        "bellman_ford.cc",
        "dijkstra.cc",
        "shortestpaths.cc",
    ],
    hdrs = ["shortestpaths.h"],
    deps = [
        ":graph",
        #        ":paths_cc_proto",
        "//ortools/base",
        "//ortools/base:adjustable_priority_queue",
        "//ortools/base:file",
        "//ortools/base:hash",
        "//ortools/base:int_type",
        #        "//ortools/base:int_type_indexed_bitmap",
        "//ortools/base:int_type_indexed_vector",
        "//ortools/base:recordio",
        "//ortools/base:sparsetable",
        "//ortools/base:strings",
        #        "//ortools/thread",
        #        "//ortools/util/coding:prefixvarint",
    ],
)

#cc_library(
#    name = "arc_dijkstra",
#    hdrs = ["arc_dijkstra.h"],
#    deps = [
#        "//ortools/base",
#        "//ortools/base:iterator_adaptors",
#    ],
#)

#cc_library(
#    name = "bounded_dijkstra",
#    hdrs = ["bounded_dijkstra.h"],
#    deps = [
#        "//ortools/base",
#        "//ortools/base:iterator_adaptors",
#        "//ortools/thread",
#    ],
#)

#cc_library(
#    name = "bidirectional_dijkstra",
#    hdrs = ["bidirectional_dijkstra.h"],
#    deps = [
#        "//ortools/base",
#        "//ortools/base:iterator_adaptors",
#        "//ortools/base:strings",
#        "//ortools/thread",
#    ],
#)

cc_library(
    name = "cliques",
    srcs = ["cliques.cc"],
    hdrs = ["cliques.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:int_type",
        "//ortools/base:int_type_indexed_vector",
        "//ortools/base:strings",
        "//ortools/util:time_limit",
    ],
)

cc_library(
    name = "hamiltonian_path",
    hdrs = ["hamiltonian_path.h"],
    deps = [
        "//ortools/base",
        "//ortools/util:bitset",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:vector_or_function",
    ],
)

cc_library(
    name = "christofides",
    hdrs = ["christofides.h"],
    deps = [
        ":eulerian_path",
        ":minimum_spanning_tree",
        "//ortools/base",
        "//ortools/linear_solver",
        "//ortools/linear_solver:linear_solver_cc_proto",
    ],
)

cc_library(
    name = "eulerian_path",
    hdrs = ["eulerian_path.h"],
    deps = [
        "//ortools/base",
    ],
)

cc_library(
    name = "minimum_spanning_tree",
    hdrs = ["minimum_spanning_tree.h"],
    deps = [
        ":connectivity",
        ":graph",
        "//ortools/base",
        "//ortools/base:adjustable_priority_queue",
        "//ortools/util:vector_or_function",
    ],
)

cc_library(
    name = "one_tree_lower_bound",
    hdrs = ["one_tree_lower_bound.h"],
    deps = [
        ":christofides",
        ":minimum_spanning_tree",
        "//ortools/base",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "ebert_graph",
    hdrs = ["ebert_graph.h"],
    deps = [
        "//ortools/base",
        #        "//ortools/testing/production_stub/public:gunit_prod",
        "//ortools/util:permutation",
        "//ortools/util:zvector",
    ],
)

#cc_library(
#    name = "strongly_connected_components",
#    srcs = ["strongly_connected_components.cc"],
#    hdrs = ["strongly_connected_components.h"],
#    deps = [
#        ":digraph",
#        ":ebert_graph",
#        "//ortools/base",
#        "//ortools/base:int_type",
#        "//ortools/base:int_type_indexed_vector",
#    ],
#)

#cc_library(
#    name = "shortest_paths",
#    srcs = ["shortest_paths.cc"],
#    hdrs = ["shortest_paths.h"],
#    deps = [
#        ":ebert_graph",
#        ":graph",
#        "//ortools/base",
#        "//ortools/base:adjustable_priority_queue",
#        "//ortools/base:file",
#        "//ortools/base:map_util",
#        "//ortools/base:stl_util",
#        "//ortools/thread",
#        "//ortools/util:zvector",
#    ],
#)

cc_library(
    name = "connectivity",
    hdrs = ["connectivity.h"],
    deps = [
        "//ortools/base",
    ],
)

proto_library(
    name = "flow_problem_proto",
    srcs = ["flow_problem.proto"],
)

cc_proto_library(
    name = "flow_problem_cc_proto",
    deps = [":flow_problem_proto"],
)

cc_library(
    name = "max_flow",
    srcs = ["max_flow.cc"],
    hdrs = ["max_flow.h"],
    deps = [
        ":ebert_graph",
        ":flow_problem_cc_proto",
        ":graph",
        ":graphs",
        "//ortools/base",
        "//ortools/util:stats",
        "//ortools/util:zvector",
    ],
)

cc_library(
    name = "min_cost_flow",
    srcs = ["min_cost_flow.cc"],
    hdrs = ["min_cost_flow.h"],
    deps = [
        ":connectivity",
        ":ebert_graph",
        ":graph",
        ":graphs",
        ":max_flow",
        "//ortools/base",
        "//ortools/base:mathutil",
        "//ortools/util:stats",
        "//ortools/util:zvector",
    ],
)

cc_library(
    name = "assignment",
    srcs = ["assignment.cc"],
    hdrs = ["assignment.h"],
    deps = [
        ":ebert_graph",
        ":linear_assignment",
        "//ortools/base",
    ],
)

cc_library(
    name = "linear_assignment",
    srcs = ["linear_assignment.cc"],
    hdrs = ["linear_assignment.h"],
    deps = [
        ":ebert_graph",
        "//ortools/base",
        "//ortools/base:strings",
        #        "//ortools/testing/production_stub/public:gunit_prod",
        "//ortools/util:permutation",
        "//ortools/util:zvector",
    ],
)

#cc_library(
#    name = "biconnected",
#    srcs = ["biconnected.cc"],
#    hdrs = ["biconnected.h"],
#    deps = [
#        ":ebert_graph",
#        "//ortools/base",
#        "//ortools/util:packed_array",
#    ],
#)

#cc_library(
#    name = "hopcroft_karp",
#    srcs = ["hopcroft_karp.c"],
#    hdrs = ["hopcroft_karp.h"],
#)

#cc_library(
#    name = "dag_connectivity",
#    srcs = ["dag_connectivity.cc"],
#    hdrs = ["dag_connectivity.h"],
#    deps = [
#        "//ortools/base",
#        "//ortools/util:bitset",
#        "//ortools/util/graph:topologicalsorter",
#    ],
#)
