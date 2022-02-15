"""Helper macro to compile and test code samples."""

def code_sample_cc(name):
    native.cc_binary(
        name = name,
        srcs = [name + ".cc"],
        deps = [
            "//ortools/sat:cp_model",
            "//ortools/sat:cp_model_solver",
            "//ortools/util:sorted_interval_list",
            "@com_google_absl//absl/types:span",
        ],
    )

    native.cc_test(
        name = name + "_test",
        size = "small",
        srcs = [name + ".cc"],
        deps = [
            ":" + name,
            "//ortools/sat:cp_model",
            "//ortools/sat:cp_model_solver",
            "//ortools/util:sorted_interval_list",
            "@com_google_absl//absl/types:span",
        ],
    )
