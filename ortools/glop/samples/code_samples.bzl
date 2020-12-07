def code_sample_cc(sample):
  native.cc_binary(
      name = sample,
      srcs = [sample + ".cc"],
      deps = [
        "//ortools/base",
        "//ortools/glop:lp_solver",
        "//ortools/lp_data",
      ],
  )

  native.cc_test(
      name = sample+"_test",
      size = "small",
      srcs = [sample + ".cc"],
      deps = [
        ":"+sample,
        "//ortools/base",
        "//ortools/glop:lp_solver",
        "//ortools/lp_data",
      ],
  )

