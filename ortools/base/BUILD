package(default_visibility = ["//visibility:public"])

cc_library(
    name = "base",
    hdrs = [
        "basictypes.h",
        "casts.h",
        "commandlineflags.h",
        "integral_types.h",
        "logging.h",
        "macros.h",
        "port.h",
        "strtoint.h",  # Move to a separate library?
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
    name = "strings",
    srcs = [
        "join.cc",
        "numbers.cc",
        "split.cc",
        "string_view.cc",
        "stringprintf.cc",
    ],
    hdrs = [
        "join.h",
        "numbers.h",
        "split.h",
        "string_view.h",
        "stringpiece_utils.h",
        "stringprintf.h",
        "strutil.h",
    ],
    deps = [
        ":base",
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
        ":strings",
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
        ":strings",
        "@com_google_protobuf_cc//:protobuf",
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
    deps = [
        ":base",
        ":file",
        ":status",
        ":strings",
        "@com_google_protobuf_cc//:protobuf",
    ],
)

cc_library(
    name = "callback",
    hdrs = [
        "callback.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "filelinereader",
    srcs = [
        "filelinereader.cc",
    ],
    hdrs = [
        "filelinereader.h",
    ],
    deps = [
        ":base",
        ":callback",
        ":file",
        ":status",
        ":strings",
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
        ":strings",
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
        ":strings",
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
    name = "stl_util",
    hdrs = [
        "stl_util.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "sparsetable",
    hdrs = [
        "sparsetable.h",
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
    name = "strongly_connected_components",
    hdrs = [
        "strongly_connected_components.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "timer",
    srcs = [
        "time_support.cc",
        "timer.cc",
    ],
    hdrs = [
        "time_support.h",
        "timer.h",
    ],
    deps = [
        ":base",
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
    name = "mutex",
    srcs = [
        "mutex.cc",
    ],
    hdrs = [
        "mutex.h",
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
        ":mutex",
    ],
)

cc_library(
    name = "inlined_vector",
    hdrs = [
        "inlined_vector.h",
    ],
    deps = [
        ":base",
    ],
)

cc_library(
    name = "span",
    hdrs = [
        "span.h",
    ],
    deps = [
        ":base",
        ":inlined_vector",
    ],
)
