def code_sample_cc(sample):
  native.cc_binary(
      name = sample,
      srcs = [sample + ".cc"],
      deps = [
        "//ortools/base",
        "//ortools/graph:min_cost_flow",
        "//ortools/graph:max_flow",
        "//ortools/graph:shortestpaths",
        "//ortools/graph:ebert_graph",
        "//ortools/graph:linear_assignment",
      ],
  )

  native.cc_test(
      name = sample+"_test",
      size = "small",
      srcs = [sample + ".cc"],
      deps = [
        ":"+sample,
        "//ortools/base",
        "//ortools/graph:min_cost_flow",
        "//ortools/graph:max_flow",
        "//ortools/graph:shortestpaths",
        "//ortools/graph:ebert_graph",
        "//ortools/graph:linear_assignment",
      ],
  )

