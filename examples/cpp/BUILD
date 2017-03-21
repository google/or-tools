cc_library(
    name = "jobshop_reader",
    hdrs = ["jobshop.h"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//util",
    ],
)

cc_library(
    name = "flexible_jobshop_reader",
    hdrs = ["flexible_jobshop.h"],
    deps = [
        "@or_tools_libraries//base",
        "@or_tools_libraries//util",
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
        "@or_tools_libraries//util",
        "@or_tools_libraries//sat:disjunctive",
        "@or_tools_libraries//sat:integer",
        "@or_tools_libraries//sat:intervals",
        "@or_tools_libraries//sat:model",
        "@or_tools_libraries//sat:optimization",
        "@or_tools_libraries//sat:precedences",
        "@or_tools_libraries//sat:sat_solver",
    ],
)