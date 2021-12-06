package(default_visibility = ["//visibility:public"])

config_setting(
    name = "on_linux",
    constraint_values = [
        "@platforms//os:linux",
    ],
)

config_setting(
    name = "on_macos",
    constraint_values = [
        "@platforms//os:macos",
    ],
)

config_setting(
    name = "on_windows",
    constraint_values = [
        "@platforms//os:windows",
    ],
)

cc_library(
    name = "base",
    srcs = [
        "commandlineflags.cc",
        "logging.cc",
        "logging_utilities.cc",
        "raw_logging.cc",
        "version.cc",
        "vlog_is_on.cc",
    ],
    hdrs = [
        "basictypes.h",
        "commandlineflags.h",
        "integral_types.h",
        "log_severity.h",
        "logging.h",
        "logging_export.h",
        "logging_utilities.h",
        "macros.h",
        "raw_logging.h",
        "stl_logging.h",
        "version.h",
        "vlog_is_on.h",
    ],
    copts = [
        "-DOR_TOOLS_MAJOR=9999",
        "-DOR_TOOLS_MINOR=0",
        "-DOR_TOOLS_PATCH=0",
    ],
    defines = select({
        "on_linux": [],
        "on_macos": [],
        "on_windows": ["OR_TOOLS_EXPORTS"],  # For logging_export.h
        "//conditions:default": [],
    }),
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/debugging:failure_signal_handler",
        "@com_google_absl//absl/debugging:stacktrace",
        "@com_google_absl//absl/debugging:symbolize",
        "@com_google_absl//absl/flags:commandlineflag",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "case",
    srcs = [
        "case.cc",
    ],
    hdrs = [
        "case.h",
    ],
    deps = [
        ":base",
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
    name = "status_macros",
    hdrs = [
        "status_macros.h",
    ],
    deps = [
        ":base",
        ":status_builder",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
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
    linkopts = select({
        "on_linux": ["-lz"],
        "on_macos": ["-lz"],
        "on_windows": [],
        "//conditions:default": ["-lz"],
    }),
    deps = [
        ":base",
        ":file",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ] + select({
        "on_linux": [],
        "on_macos": [],
        "on_windows": ["@zlib//:zlib"],
        "//conditions:default": [],
    }),
)

cc_library(
    name = "protoutil",
    hdrs = [
        "protoutil.h",
    ],
    deps = [
        ":timer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
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
    name = "strong_vector",
    hdrs = ["strong_vector.h"],
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
    name = "linked_hash_map",
    hdrs = ["linked_hash_map.h"],
    deps = [
        ":base",
        "@com_google_absl//absl/container:common",
        "@com_google_absl//absl/container:flat_hash_map",
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
    linkopts = select({
        "on_linux": ["-Wl,--no-as-needed -ldl"],
        "on_macos": [],
        "on_windows": [],
        "//conditions:default": [],
    }),
    deps = [":base"],
)

cc_library(
    name = "gzipstring",
    hdrs = ["gzipstring.h"],
    deps = [":base"] + select({
        "on_windows": ["@zlib//:zlib"],
        "//conditions:default": [],
    }),
)
