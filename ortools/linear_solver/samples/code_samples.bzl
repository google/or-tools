"""Helper macro to compile and test code samples."""

def code_sample_cc(name):
  native.cc_binary(
      name = name + "_cc",
      srcs = [name + ".cc"],
      deps = [
        "//ortools/base",
        "//ortools/linear_solver",
        "//ortools/linear_solver:linear_solver_cc_proto",
      ],
  )

  native.cc_test(
      name = name + "_cc_test",
      size = "small",
      srcs = [name + ".cc"],
      deps = [
        ":" + name + "_cc",
        "//ortools/base",
        "//ortools/linear_solver",
        "//ortools/linear_solver:linear_solver_cc_proto",
      ],
  )

