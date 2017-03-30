cc_binary(
    name = "cryptarithm",
    srcs = ["cryptarithm.cc"],
    deps = [
        "@com_google_protobuf_cc//:protobuf",
        "@or_tools_libraries//base",
        "@or_tools_libraries//constraint_solver:cp",
    ],
)

cc_library(
    name = "jobshop_reader",
    hdrs = ["jobshop.h"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//util:filelineiter",
    ],
)

cc_library(
    name = "flexible_jobshop_reader",
    hdrs = ["flexible_jobshop.h"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//util:filelineiter",
        "@or_tools_libraries//util:string_array",
    ],
)

cc_binary(
    name = "jobshop_sat",
    srcs = [
        "jobshop_sat.cc",
    ],
    deps = [
        ":flexible_jobshop_reader",
        ":jobshop_reader",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:file",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//base:timer",
        "@or_tools_libraries//sat:disjunctive",
        "@or_tools_libraries//sat:integer",
        "@or_tools_libraries//sat:intervals",
        "@or_tools_libraries//sat:model",
        "@or_tools_libraries//sat:optimization",
        "@or_tools_libraries//sat:precedences",
        "@or_tools_libraries//sat:sat_solver",
    ],
)

cc_binary(
    name = "linear_programming",
    srcs = ["linear_programming.cc"],
    copts = ["-DUSE_GLOP"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//linear_solver:linear_solver",
        "@or_tools_libraries//linear_solver:linear_solver_cc_proto",
    ],
)

cc_binary(
    name = "sat_runner",
    srcs = [
        "opb_reader.h",
        "sat_cnf_reader.h",
        "sat_runner.cc",
    ],
    deps = [
        "@com_google_protobuf_cc//:protobuf",
        "@or_tools_libraries//sat:boolean_problem",
        "@or_tools_libraries//sat:boolean_problem_cc_proto",
        #        "@or_tools_libraries//sat:cp_model_proto",
        #        "@or_tools_libraries//sat:cp_model_solver",
        "@or_tools_libraries//sat:drat",
        "@or_tools_libraries//sat:lp_utils",
        "@or_tools_libraries//sat:optimization",
        "@or_tools_libraries//sat:sat_solver",
        "@or_tools_libraries//sat:simplification",
        "@or_tools_libraries//sat:symmetry",
        "@or_tools_libraries//base",
        "@or_tools_libraries//base:file",
        "@or_tools_libraries//base:strings",
        "@or_tools_libraries//base:status",
        "@or_tools_libraries//base:random",
        "@or_tools_libraries//base:threadpool",
        "@or_tools_libraries//algorithms:sparse_permutation",
        "@or_tools_libraries//lp_data:mps_reader",
        "@or_tools_libraries//lp_data:proto_utils",
        "@or_tools_libraries//util:filelineiter",
        "@or_tools_libraries//util:time_limit",
    ],
)

cc_binary(
    name = "solve",
    srcs = ["solve.cc"],
    copts = [
        "-DUSE_GLOP",
        "-DUSE_BOP",
    ],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//linear_solver:linear_solver",
        "@or_tools_libraries//linear_solver:linear_solver_cc_proto",
        "@or_tools_libraries//lp_data:mps_reader",
    ],
)

cc_binary(
    name = "tsp",
    srcs = ["tsp.cc"],
    deps = [
        "@com_google_protobuf_cc//:protobuf",
        "@or_tools_libraries//base",
        "@or_tools_libraries//constraint_solver:cp",
        "@or_tools_libraries//constraint_solver:routing",
        "@or_tools_libraries//constraint_solver:routing_flags",
    ],
)
