def code_sample_cc(sample):
  native.cc_binary(
      name = sample,
      srcs = [sample + ".cc"],
      deps = [
          "//ortools/algorithms:knapsack_solver_lib",
      ],
  )

