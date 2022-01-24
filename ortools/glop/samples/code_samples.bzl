"""Helper macro to compile and test code samples."""

def code_sample_cc(name):
  native.cc_binary(
      name = name,
      srcs = [name + ".cc"],
      deps = [
        "//ortools/base",
        "//ortools/glop:lp_solver",
        "//ortools/lp_data",
      ],
  )

  native.cc_test(
      name = name+"_test",
      size = "small",
      srcs = [name + ".cc"],
      deps = [
        ":"+name,
        "//ortools/base",
        "//ortools/glop:lp_solver",
        "//ortools/lp_data",
      ],
  )
