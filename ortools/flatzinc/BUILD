package(default_visibility = ["//visibility:public"])

cc_library(
    name = "logging",
    srcs = ["logging.cc"],
    hdrs = ["logging.h"],
    deps = [
        "//ortools/base",
    ],
)

cc_library(
    name = "model",
    srcs = ["model.cc"],
    hdrs = ["model.h"],
    deps = [
        ":logging",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/base:stl_util",
        "//ortools/base:strings",
        "//ortools/util:iterators",
        "//ortools/util:string_array",
    ],
)

#cc_test(
#    name = "model_test",
#    size = "small",
#    srcs = ["model_test.cc"],
#    deps = [
#        ":model",
#        "//@gtest//:main",
#    ],
#)

cc_library(
    name = "parser_yacc_lib",
    srcs = ["parser.tab.cc"],
    hdrs = [
        "parser.tab.hh",
        "parser_util.cc",
        "parser_util.h",
    ],
    copts = [
        #        "-Xclang-only=-Wno-implicit-fallthrough",
        "$(STACK_FRAME_UNLIMITED)",  # parser.tab.cc
    ],
    deps = [
        ":model",
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/base:stl_util",
        "//ortools/base:strings",
        "//ortools/util:string_array",
    ],
)

cc_library(
    name = "parser_lex_lib",
    srcs = ["parser.yy.cc"],
    copts = [
        #        "-Xclang-only=-Wno-implicit-fallthrough",
        "-Wno-unused-function",  # parser.yy.cc
    ],
    deps = [
        ":parser_yacc_lib",
        "//ortools/base",
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

#cc_test(
#    name = "parser_test",
#    size = "small",
#    srcs = ["parser_test.cc"],
#    data = [
#        "testdata/M12.fzn",
#        "testdata/alpha.fzn",
#        "testdata/golomb.fzn",
#    ],
#    deps = [
#        ":model",
#        ":parser_lib",
#        "//@gtest//:main",
#        "//ortools/base:file",
#        "//ortools/base:file:path",
#    ],
#)

cc_library(
    name = "presolve",
    srcs = ["presolve.cc"],
    hdrs = ["presolve.h"],
    deps = [
        ":logging",
        ":model",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/base:strings",
        "//ortools/graph:cliques",
        "//ortools/util:saturated_arithmetic",
        "//ortools/util:vector_map",
    ],
)

#cc_test(
#    name = "presolve_test",
#    size = "small",
#    srcs = ["presolve_test.cc"],
#    deps = [
#        ":model",
#        ":presolve",
#        "//@gtest//:main",
#        "//ortools/base:file",
#        "//ortools/base:file:path",
#    ],
#)

cc_library(
    name = "checker",
    srcs = ["checker.cc"],
    hdrs = ["checker.h"],
    deps = [
        ":logging",
        ":model",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:map_util",
        "//ortools/base:strings",
    ],
)

cc_library(
    name = "sat_constraint",
    srcs = ["sat_constraint.cc"],
    hdrs = ["sat_constraint.h"],
    deps = [
        ":logging",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:int_type",
        "//ortools/base:int_type_indexed_vector",
        "//ortools/base:map_util",
        "//ortools/constraint_solver:cp",
        "//ortools/sat:pb_constraint",
        "//ortools/sat:sat_base",
        "//ortools/sat:sat_solver",
        "//ortools/util:string_array",
    ],
)

cc_library(
    name = "flatzinc_constraints",
    srcs = ["flatzinc_constraints.cc"],
    hdrs = ["flatzinc_constraints.h"],
    deps = [
        ":logging",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/constraint_solver:cp",
        "//ortools/util:string_array",
    ],
)

cc_library(
    name = "reporting",
    srcs = ["reporting.cc"],
    hdrs = ["reporting.h"],
    deps = [
        ":logging",
        "//ortools/base",
        "//ortools/base:mutex",
        "//ortools/constraint_solver:cp",
    ],
)

cc_library(
    name = "solver_util",
    srcs = ["solver_util.cc"],
    hdrs = ["solver_util.h"],
    deps = [
        ":logging",
        ":model",
        "//ortools/base",
        "//ortools/constraint_solver:cp",
    ],
)

cc_library(
    name = "solver_data",
    srcs = ["solver_data.cc"],
    hdrs = ["solver_data.h"],
    deps = [
        ":logging",
        ":model",
        ":sat_constraint",
        "//ortools/base",
        "//ortools/constraint_solver:cp",
    ],
)

cc_library(
    name = "constraints",
    srcs = ["constraints.cc"],
    hdrs = ["constraints.h"],
    deps = [
        ":flatzinc_constraints",
        ":logging",
        ":model",
        ":sat_constraint",
        ":solver_data",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/constraint_solver:cp",
        "//ortools/util:string_array",
    ],
)

cc_library(
    name = "solver",
    srcs = [
        "solver.cc",
    ],
    hdrs = [
        "solver.h",
    ],
    deps = [
        ":checker",
        ":constraints",
        ":logging",
        ":model",
        ":reporting",
        ":sat_constraint",
        ":solver_data",
        ":solver_util",
        "//ortools/base",
        "//ortools/base:hash",
        "//ortools/base:int_type",
        "//ortools/base:int_type_indexed_vector",
        "//ortools/base:map_util",
        "//ortools/constraint_solver:cp",
        "//ortools/sat:pb_constraint",
        "//ortools/sat:sat_base",
        "//ortools/sat:sat_solver",
        "//ortools/util:string_array",
    ],
)

cc_library(
    name = "sat_fz_solver",
    srcs = ["sat_fz_solver.cc"],
    hdrs = ["sat_fz_solver.h"],
    deps = [
        ":checker",
        ":logging",
        ":model",
        ":solver",
        "//ortools/base",
        "//ortools/base:map_util",
        "//ortools/sat:cp_constraints",
        "//ortools/sat:cumulative",
        "//ortools/sat:disjunctive",
        "//ortools/sat:flow_costs",
        "//ortools/sat:integer",
        "//ortools/sat:integer_expr",
        "//ortools/sat:intervals",
        "//ortools/sat:linear_programming_constraint",
        "//ortools/sat:model",
        "//ortools/sat:optimization",
        "//ortools/sat:sat_solver",
        "//ortools/sat:table",
        "//ortools/util:sorted_interval_list",
    ],
)

cc_library(
    name = "cp_model_fz_solver",
    srcs = ["cp_model_fz_solver.cc"],
    hdrs = ["cp_model_fz_solver.h"],
    deps = [
        ":checker",
        ":logging",
        ":model",
        ":solver",
        "//ortools/base",
        "//ortools/base:strings",
        "//ortools/base:map_util",
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
    ],
)
cc_binary(
    name = "fz",
    srcs = ["fz.cc"],
    deps = [
        ":cp_model_fz_solver",
        ":logging",
        ":model",
        ":parser_lib",
        ":presolve",
        ":reporting",
        ":sat_fz_solver",
        ":solver",
        ":solver_util",
        "//ortools/base",
        "//ortools/base:threadpool",
    ],
)
