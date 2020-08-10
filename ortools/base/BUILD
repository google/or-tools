package(default_visibility = ["//visibility:public"])

config_setting(
    name = "on_linux",
    constraint_values = [
        "@platforms//os:linux",
    ],
)

cc_library(
    name = "base",
    hdrs = [
        "basictypes.h",
        "commandlineflags.h",
        "integral_types.h",
        "logging.h",
        "macros.h",
	"version.h",
    ],
    srcs = [
        "logging.cc",
        "version.cc",
    ],
    copts = [
        "-DOR_TOOLS_MAJOR=7",
        "-DOR_TOOLS_MINOR=8",
    ],
    deps = [
        "@com_github_gflags_gflags//:gflags",
        "@com_github_glog_glog//:glog",
    ],
)

cc_library(
    name = "container_logging",
    hdrs = [
        "container_logging.h",
    ],
    deps = [
        ":base",
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
    name = "statusor",
    hdrs = [
        "statusor.h",
    ],
    deps = [
        ":base",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "status_macros",
    hdrs = [
        "status_macros.h",
    ],
    deps = [
        ":base",
        ":statusor",
        "@com_google_absl//absl/status",
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
        "@com_google_absl//absl/strings",
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
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "filelineiter",
    hdrs = ["filelineiter.h"],
    deps = [
        ":base",
        ":file",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
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
        ":statusor",

        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "protoutil",
    hdrs = [
        "protoutil.h",
    ],
    deps = [
        ":statusor",
        ":timer",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
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
        "@com_google_absl//absl/strings",
        #"@com_google_protobuf//:protobuf",
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
        "@com_google_absl//absl/random",
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
        "@com_google_absl//absl/strings",
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
    name = "status_builder",
    hdrs = [
        "status_builder.h",
    ],
    deps = [
        ":base",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "protobuf_util",
    hdrs = [
        "protobuf_util.h",
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

cc_library(
    name = "dynamic_library",
    hdrs = ["dynamic_library.h"],
    deps = [":base"],
    linkopts = select({
        "on_linux": ["-Wl,--no-as-needed -ldl"],
        "//conditions:default": [],
    }),
)
