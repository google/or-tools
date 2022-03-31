"""Helper macro to compile and test code samples."""

load("@rules_python//python:defs.bzl", "py_binary")
load("@ortools_deps//:requirements.bzl", "requirement")

def code_sample_cc(name):
    native.cc_binary(
        name = name + "_cc",
        srcs = [name + ".cc"],
        deps = [
            "//ortools/sat:cp_model",
            "//ortools/sat:cp_model_solver",
            "//ortools/util:sorted_interval_list",
            "@com_google_absl//absl/types:span",
        ],
    )

    native.cc_test(
        name = name + "_cc_test",
        size = "small",
        srcs = [name + ".cc"],
        args = [name],
        deps = [
            ":" + name + "_cc",
            "//ortools/sat:cp_model",
            "//ortools/sat:cp_model_solver",
            "//ortools/util:sorted_interval_list",
            "@com_google_absl//absl/types:span",
        ],
    )

def code_sample_py(name):
    py_binary(
        name = name + "_py3",
        srcs = [name + ".py"],
        main = name + ".py",
        deps = [
            requirement("absl-py"),
            "//ortools/sat/python:cp_model",
        ],
        python_version = "PY3",
        srcs_version = "PY3",
    )

    native.sh_test(
        name = name + "_py_test",
        size = "small",
        srcs = ["code_samples_py_test.sh"],
        args = [name],
        data = [
            ":" + name + "_py3",
        ],
    )

def code_sample_cc_py(name):
    code_sample_cc(name = name)
    code_sample_py(name = name)
