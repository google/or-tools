"""Helper macro to compile and test code samples."""

load("@rules_python//python:defs.bzl", "py_binary")
load("@ortools_deps//:requirements.bzl", "requirement")

def code_sample_cc(name):
    native.cc_binary(
        name = name + "_cc",
        srcs = [name + ".cc"],
        deps = [
            "//ortools/base",
            "//ortools/graph:assignment",
            "//ortools/graph:ebert_graph",
            "//ortools/graph:linear_assignment",
            "//ortools/graph:max_flow",
            "//ortools/graph:min_cost_flow",
            "//ortools/graph:shortestpaths",
        ],
    )

    native.sh_test(
        name = name + "_cc_test",
        size = "small",
        srcs = ["code_samples_cc_test.sh"],
        args = [name],
        data = [
            ":" + name + "_cc",
        ],
    )

def code_sample_py(name):
    py_binary(
        name = name + "_py3",
        srcs = [name + ".py"],
        main = name + ".py",
        data = [
            "//ortools/graph/python:linear_sum_assignment.so",
            "//ortools/graph/python:min_cost_flow.so",
            "//ortools/graph/python:max_flow.so",
        ],
        deps = [
            requirement("absl-py"),
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
