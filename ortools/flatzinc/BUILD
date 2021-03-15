# Description:
# Source code for the flatzinc interpreter. See www.minizinc.org.
# This is a google owned implementation.

package(default_visibility = ["//visibility:public"])

# ---------------------------------------------------------------------------
# Baseline flatzinc model.
# ---------------------------------------------------------------------------

cc_library(
    name = "model",
    srcs = ["model.cc"],
    hdrs = ["model.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/base:stl_util",
        "//ortools/graph:iterators",
        "//ortools/util:logging",
        "//ortools/util:string_array",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "parser_yacc_lib",
    srcs = ["parser.tab.cc"],
    hdrs = [
        "parser.tab.hh",
        "parser_util.cc",
        "parser_util.h",
    ],
    copts = [
        "$(STACK_FRAME_UNLIMITED)",  # parser.tab.cc
    ],
    deps = [
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/base:mathutil",
        "//ortools/base:stl_util",
        "//ortools/util:string_array",
        ":model",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "parser_lex_lib",
    srcs = ["parser.yy.cc"],
    copts = [
        "-Wno-unused-function",  # parser.yy.cc
    ],
    deps = [
        "//ortools/base",
        ":parser_yacc_lib",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "parser_lib",
    srcs = ["parser.cc"],
    hdrs = ["parser.h"],
    deps = [
        ":model",
        ":parser_lex_lib",
        ":parser_yacc_lib",
    ],
)

cc_library(
    name = "presolve",
    srcs = ["presolve.cc"],
    hdrs = ["presolve.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/graph:cliques",
        "//ortools/util:logging",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:vector_map",
        ":model",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "checker",
    srcs = ["checker.cc"],
    hdrs = ["checker.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/util:logging",
        ":model",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "cp_model_fz_solver",
    srcs = ["cp_model_fz_solver.cc"],
    hdrs = ["cp_model_fz_solver.h"],
    deps = [
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/base:threadpool",
        "//ortools/sat:cp_constraints",
        "//ortools/sat:cp_model_cc_proto",
        "//ortools/sat:cp_model_solver",
        "//ortools/sat:cp_model_utils",
        "//ortools/sat:cumulative",
        "//ortools/sat:disjunctive",
        "//ortools/sat:integer",
        "//ortools/sat:integer_expr",
        "//ortools/sat:intervals",
        "//ortools/sat:model",
        "//ortools/sat:optimization",
        "//ortools/sat:sat_solver",
        "//ortools/sat:table",
        "//ortools/util:logging",
        ":checker",
        ":model",
        "@com_google_absl//absl/strings",
    ],
)

cc_binary(
    name = "fz",
    srcs = ["fz.cc"],
    deps = [
        "//ortools/base",
        "//ortools/base:threadpool",
        "//ortools/util:logging",
        ":cp_model_fz_solver",
        ":model",
        ":parser_lib",
        ":presolve",
    ],
)
