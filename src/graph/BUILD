package(default_visibility = ["//visibility:public"])

cc_library(
    name = "graph",
    hdrs = ["graph.h"],
    deps = [
        "//base",
        "//util:iterators",
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
        "//base:hash",
        "//base:map_util",
    ],
)

cc_library(
    name = "io",
    hdrs = ["io.h"],
    deps = [
        ":graph",
        "//base:status",
        "//base:statusor",
        "//base:strings",
        "//util:filelineiter",
    ],
)

#cc_library(
#    name = "digraph",
#    srcs = ["digraph.cc"],
#    hdrs = ["digraph.h"],
#    deps = [
#        "//base",
#        "//base:int_type",
#        "//base:int_type_indexed_vector",
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
        "//base",
        "//base:adjustable_priority_queue",
        "//base:file",
        "//base:hash",
        "//base:int_type",
        #        "//base:int_type_indexed_bitmap",
        "//base:int_type_indexed_vector",
        "//base:recordio",
        "//base:sparsetable",
        "//base:strings",
        #        "//thread",
        #        "//util/coding:prefixvarint",
    ],
)

#cc_library(
#    name = "arc_dijkstra",
#    hdrs = ["arc_dijkstra.h"],
#    deps = [
#        "//base",
#        "//base:iterator_adaptors",
#    ],
#)

#cc_library(
#    name = "bounded_dijkstra",
#    hdrs = ["bounded_dijkstra.h"],
#    deps = [
#        "//base",
#        "//base:iterator_adaptors",
#        "//thread",
#    ],
#)

#cc_library(
#    name = "bidirectional_dijkstra",
#    hdrs = ["bidirectional_dijkstra.h"],
#    deps = [
#        "//base",
#        "//base:iterator_adaptors",
#        "//base:strings",
#        "//thread",
#    ],
#)

cc_library(
    name = "cliques",
    srcs = ["cliques.cc"],
    hdrs = ["cliques.h"],
    deps = [
        "//base",
        "//base:hash",
        "//base:int_type",
        "//base:int_type_indexed_vector",
        "//base:strings",
        "//util:time_limit",
    ],
)

cc_library(
    name = "hamiltonian_path",
    hdrs = ["hamiltonian_path.h"],
    deps = [
        "//base",
        "//util:bitset",
        "//util:saturated_arithmetic",
        "//util:vector_or_function",
    ],
)

cc_library(
    name = "christofides",
    hdrs = ["christofides.h"],
    deps = [
        ":eulerian_path",
        ":minimum_spanning_tree",
        "//base",
        "//linear_solver:linear_solver_cc_proto",
        "//linear_solver:linear_solver_glop",
        #        "//linear_solver:linear_solver_wrapper",
    ],
)

cc_library(
    name = "eulerian_path",
    hdrs = ["eulerian_path.h"],
    deps = [
        "//base",
    ],
)

cc_library(
    name = "minimum_spanning_tree",
    hdrs = ["minimum_spanning_tree.h"],
    deps = [
        ":connectivity",
        ":graph",
        "//base",
        "//base:adjustable_priority_queue",
        "//util:vector_or_function",
    ],
)

cc_library(
    name = "one_tree_lower_bound",
    hdrs = ["one_tree_lower_bound.h"],
    deps = [
        ":christofides",
        ":minimum_spanning_tree",
        "//base",
        "//base:strings",
    ],
)

cc_library(
    name = "ebert_graph",
    hdrs = ["ebert_graph.h"],
    deps = [
        "//base",
        #        "//testing/production_stub/public:gunit_prod",
        "//util:permutation",
        "//util:zvector",
    ],
)

#cc_library(
#    name = "strongly_connected_components",
#    srcs = ["strongly_connected_components.cc"],
#    hdrs = ["strongly_connected_components.h"],
#    deps = [
#        ":digraph",
#        ":ebert_graph",
#        "//base",
#        "//base:int_type",
#        "//base:int_type_indexed_vector",
#    ],
#)

#cc_library(
#    name = "shortest_paths",
#    srcs = ["shortest_paths.cc"],
#    hdrs = ["shortest_paths.h"],
#    deps = [
#        ":ebert_graph",
#        ":graph",
#        "//base",
#        "//base:adjustable_priority_queue",
#        "//base:dense_hash_map",
#        "//base:file",
#        "//base:map_util",
#        "//base:stl_util",
#        "//thread",
#        "//util:zvector",
#    ],
#)

cc_library(
    name = "connectivity",
    hdrs = ["connectivity.h"],
    deps = [
        "//base",
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
        "//base",
        "//util:stats",
        "//util:zvector",
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
        "//base",
        "//base:mathutil",
        "//util:stats",
        "//util:zvector",
    ],
)

cc_library(
    name = "assignment",
    srcs = ["assignment.cc"],
    hdrs = ["assignment.h"],
    deps = [
        ":ebert_graph",
        ":linear_assignment",
        "//base",
    ],
)

cc_library(
    name = "linear_assignment",
    srcs = ["linear_assignment.cc"],
    hdrs = ["linear_assignment.h"],
    deps = [
        ":ebert_graph",
        "//base",
        "//base:strings",
        #        "//testing/production_stub/public:gunit_prod",
        "//util:permutation",
        "//util:zvector",
    ],
)

#cc_library(
#    name = "biconnected",
#    srcs = ["biconnected.cc"],
#    hdrs = ["biconnected.h"],
#    deps = [
#        ":ebert_graph",
#        "//base",
#        "//util:packed_array",
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
#        "//base",
#        "//util:bitset",
#        "//util/graph:topologicalsorter",
#    ],
#)
