package(default_visibility = ["//visibility:public"])

cc_library(
    name = "base",
    hdrs = [
        "basictypes.h",
        "commandlineflags.h",
        "integral_types.h",
        "logging.h",
        "macros.h",
    ],
    deps = [
        "@com_github_gflags_gflags//:gflags",
        "@com_github_glog_glog//:glog",
    ],
)

cc_library(
    name = "iterator_adaptors",
    hdrs = [
        "iterator_adaptors.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "status",
    hdrs = [
        "status.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "statusor",
    hdrs = [
        "statusor.h",
    ],
    deps = [
        ":base",
        ":status",
    ],
)

cc_library(
    name = "encodingutils",
    hdrs = [
        "encodingutils.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "ptr_util",
    hdrs = [
        "ptr_util.h",
    ],
)

cc_library(
    name = "sysinfo",
    srcs = [
        "sysinfo.cc",
    ],
    hdrs = [
        "sysinfo.h",
    ],
    deps = [
        ":base",
        "@com_google_absl//absl/strings:strings",
    ],
)

cc_library(
    name = "file",
    srcs = [
        "file.cc",
    ],
    hdrs = [
        "file.h",
    ],
    deps = [
        ":base",
        ":status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf_cc//:protobuf",
    ],
)

cc_library(
    name = "filelineiter",
    hdrs = ["filelineiter.h"],
    deps = [
        ":base",
        ":file",
        ":status",
        "@com_google_absl//absl/strings:strings",
    ],
)

cc_library(
    name = "recordio",
    srcs = [
        "recordio.cc",
    ],
    hdrs = [
        "recordio.h",
    ],
    linkopts = ["-lz"],
    deps = [
        ":base",
        ":file",
        ":status",
        "@com_google_absl//absl/strings:strings",
        "@com_google_protobuf_cc//:protobuf",
    ],
)

cc_library(
    name = "protoutil",
    hdrs = [
        "protoutil.h",
    ],
    deps = [
        ":status",
        ":statusor",
        ":timer",
        "@com_google_protobuf_cc//:protobuf",
    ],
)

cc_library(
    name = "hash",
    hdrs = [
        "hash.h",
        "thorough_hash.h",
    ],
    deps = [
        ":base",
        "@com_google_absl//absl/strings:strings",
        "@com_google_protobuf_cc//:protobuf",
    ],
)

cc_library(
    name = "typeid",
    hdrs = [
        "typeid.h",
    ],
)

cc_library(
    name = "random",
    srcs = [
        "random.cc",
    ],
    hdrs = [
        "random.h",
    ],
    deps = [
        ":base",
        ":hash",
    ],
)

cc_library(
    name = "cleanup",
    hdrs = [
        "cleanup.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "murmur",
    hdrs = [
        "murmur.h",
    ],
    deps = [
        ":base",
        ":hash",
        "@com_google_absl//absl/strings:strings",
    ],
)

cc_library(
    name = "map_util",
    hdrs = [
        "map_util.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "small_map",
    hdrs = [
        "small_map.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "small_ordered_set",
    hdrs = [
        "small_ordered_set.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "canonical_errors",
    hdrs = [
        "canonical_errors.h",
    ],
)

cc_library(
    name = "stl_util",
    hdrs = [
        "stl_util.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "accurate_sum",
    hdrs = [
        "accurate_sum.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "mathutil",
    hdrs = [
        "mathutil.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "timer",
    srcs = [
        "timer.cc",
    ],
    hdrs = [
        "timer.h",
    ],
    deps = [
        ":base",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "int_type",
    hdrs = ["int_type.h"],
    deps = [":base"],
)

cc_library(
    name = "int_type_indexed_vector",
    hdrs = ["int_type_indexed_vector.h"],
    deps = [
        ":base",
        ":int_type",
    ],
)

cc_library(
    name = "adjustable_priority_queue",
    hdrs = [
        "adjustable_priority_queue.h",
        "adjustable_priority_queue-inl.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "bitmap",
    srcs = [
        "bitmap.cc",
    ],
    hdrs = [
        "bitmap.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "threadpool",
    srcs = [
        "threadpool.cc",
    ],
    hdrs = [
        "threadpool.h",
    ],
    deps = [
        ":base",
        "@com_google_absl//absl/synchronization",
    ],
)
