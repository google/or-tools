package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "basic_example",
    srcs = ["basic_example.cc"],
    deps = [
        "//ortools/base",
        "//ortools/math_opt/cpp:math_opt",
        "//ortools/math_opt/solvers:gscip_solver",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_binary(
    name = "linear_programming",
    srcs = ["linear_programming.cc"],
    deps = [
        "//ortools/base",
        "//ortools/math_opt/cpp:math_opt",
        "//ortools/math_opt/solvers:glop_solver",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_binary(
    name = "integer_programming",
    srcs = ["integer_programming.cc"],
    deps = [
        "//ortools/base",
        "//ortools/math_opt/cpp:math_opt",
        "//ortools/math_opt/solvers:gscip_solver",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_binary(
    name = "facility_lp_benders",
    srcs = ["facility_lp_benders.cc"],
    deps = [
        "//ortools/base",
        "//ortools/math_opt/cpp:math_opt",
        "//ortools/math_opt/solvers:gurobi_solver",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/random:distributions",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
    ],
)

cc_binary(
    name = "lagrangian_relaxation",
    srcs = ["lagrangian_relaxation.cc"],
    deps = [
        "//ortools/base",
        "//ortools/base:container_logging",
        "//ortools/base:mathutil",
        "//ortools/math_opt/cpp:math_opt",
        "//ortools/math_opt/solvers:glop_solver",
        "//ortools/math_opt/solvers:gscip_solver",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)
