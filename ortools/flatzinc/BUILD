package(default_visibility = ["//visibility:public"])

cc_library(
    name = "logging",
    srcs = ["logging.cc"],
    hdrs = ["logging.h"],
    deps = [
        "//base",
    ],
)

cc_library(
    name = "model",
    srcs = ["model.cc"],
    hdrs = ["model.h"],
    deps = [
        ":logging",
        "//base",
        "//base:hash",
        "//base:map_util",
        "//base:stl_util",
        "//base:strings",
        "//util:iterators",
        "//util:string_array",
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
        "//base",
        "//base:map_util",
        "//base:stl_util",
        "//base:strings",
        "//util:string_array",
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
        "//base",
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
#        "//base:file",
#        "//base:file:path",
#    ],
#)

cc_library(
    name = "presolve",
    srcs = ["presolve.cc"],
    hdrs = ["presolve.h"],
    deps = [
        ":logging",
        ":model",
        "//base",
        "//base:hash",
        "//base:map_util",
        "//base:strings",
        "//graph:cliques",
        "//util:saturated_arithmetic",
        "//util:vector_map",
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
#        "//base:file",
#        "//base:file:path",
#    ],
#)

cc_library(
    name = "checker",
    srcs = ["checker.cc"],
    hdrs = ["checker.h"],
    deps = [
        ":logging",
        ":model",
        "//base",
        "//base:hash",
        "//base:map_util",
        "//base:strings",
    ],
)

cc_library(
    name = "sat_constraint",
    srcs = ["sat_constraint.cc"],
    hdrs = ["sat_constraint.h"],
    deps = [
        ":logging",
        "//base",
        "//base:hash",
        "//base:int_type",
        "//base:int_type_indexed_vector",
        "//base:map_util",
        "//constraint_solver:cp",
        "//sat:pb_constraint",
        "//sat:sat_base",
        "//sat:sat_solver",
        "//util:string_array",
    ],
)

cc_library(
    name = "flatzinc_constraints",
    srcs = ["flatzinc_constraints.cc"],
    hdrs = ["flatzinc_constraints.h"],
    deps = [
        ":logging",
        "//base",
        "//base:hash",
        "//constraint_solver:cp",
        "//util:string_array",
    ],
)

cc_library(
    name = "reporting",
    srcs = ["reporting.cc"],
    hdrs = ["reporting.h"],
    deps = [
        ":logging",
        "//base",
        "//base:mutex",
        "//constraint_solver:cp",
    ],
)

cc_library(
    name = "solver_util",
    srcs = ["solver_util.cc"],
    hdrs = ["solver_util.h"],
    deps = [
        ":logging",
        ":model",
        "//base",
        "//constraint_solver:cp",
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
        "//base",
        "//constraint_solver:cp",
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
        "//base",
        "//base:hash",
        "//constraint_solver:cp",
        "//util:string_array",
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
        "//base",
        "//base:hash",
        "//base:int_type",
        "//base:int_type_indexed_vector",
        "//base:map_util",
        "//constraint_solver:cp",
        "//sat:pb_constraint",
        "//sat:sat_base",
        "//sat:sat_solver",
        "//util:string_array",
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
        "//base",
        "//base:map_util",
        "//sat:cp_constraints",
        "//sat:cumulative",
        "//sat:disjunctive",
        "//sat:flow_costs",
        "//sat:integer",
        "//sat:integer_expr",
        "//sat:intervals",
        "//sat:linear_programming_constraint",
        "//sat:model",
        "//sat:optimization",
        "//sat:sat_solver",
        "//sat:table",
        "//util:sorted_interval_list",
    ],
)

cc_binary(
    name = "fz",
    srcs = ["fz.cc"],
    deps = [
        #        ":cp_model_fz_solver",
        ":logging",
        ":model",
        ":parser_lib",
        ":presolve",
        ":reporting",
        ":sat_fz_solver",
        ":solver",
        ":solver_util",
        "//base",
        "//base:threadpool",
    ],
)
