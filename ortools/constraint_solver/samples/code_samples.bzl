def code_sample_cc(sample):
  native.cc_binary(
      name = sample,
      srcs = [sample + ".cc"],
      deps = [
        "//ortools/base",
        "//ortools/constraint_solver:cp",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_enums_cc_proto",
        "//ortools/constraint_solver:routing_flags",
      ],
  )

  native.cc_test(
      name = sample+"_test",
      size = "small",
      srcs = [sample + ".cc"],
      deps = [
        ":"+sample,
        "//ortools/base",
        "//ortools/constraint_solver:cp",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_enums_cc_proto",
        "//ortools/constraint_solver:routing_flags",
      ],
  )

