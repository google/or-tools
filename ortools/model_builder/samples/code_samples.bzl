"""Helper macro to compile and test code samples."""

load("@ortools_deps//:requirements.bzl", "requirement")
load("@rules_python//python:defs.bzl", "py_binary")

def code_sample_cc(name):
  native.cc_binary(
      name = name,
      srcs = [name + ".cc"],
      deps = [
        "//ortools/base",
        "//ortools/model_builder",
        "//ortools/model_builder:linear_solver_cc_proto",
      ],
  )

  native.cc_test(
      name = name+"_test",
      size = "small",
      srcs = [name + ".cc"],
      deps = [
        ":"+name,
        "//ortools/base",
        "//ortools/linear_solver",
        "//ortools/linear_solver:linear_solver_cc_proto",
      ],
  )

def code_sample_py(name):
    py_binary(
        name = name + "_py3",
        srcs = [name + ".py"],
        main = name + ".py",
        deps = [
            requirement("absl-py"),
            "//ortools/model_builder/python:model_builder",
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

