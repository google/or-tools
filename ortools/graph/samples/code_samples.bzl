"""Helper macro to compile and test code samples."""

def code_sample_cc(name):
  native.cc_binary(
      name = name,
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

  native.cc_test(
      name = name+"_test",
      size = "small",
      srcs = [name + ".cc"],
      deps = [
        ":"+name,
        "//ortools/base",
        "//ortools/graph:assignment",
        "//ortools/graph:ebert_graph",
        "//ortools/graph:linear_assignment",
        "//ortools/graph:max_flow",
        "//ortools/graph:min_cost_flow",
        "//ortools/graph:shortestpaths",
      ],
  )
