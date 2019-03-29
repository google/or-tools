load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")

git_repository(
    name = "com_github_gflags_gflags",
    commit = "e171aa2",  # release v2.2.2
    remote = "https://github.com/gflags/gflags.git",
)

git_repository(
    name = "com_github_glog_glog",
    commit = "96a2f23",  # release v0.4.0
    remote = "https://github.com/google/glog.git",
)

git_repository(
    name = "bazel_skylib",
    commit = "3721d32",  # release 0.8.0
    remote = "https://github.com/bazelbuild/bazel-skylib.git",
)

git_repository(
    name = "com_google_protobuf",
    commit = "f425b9f",
#    commit = "6973c3a",  # release v3.7.1
    remote = "https://github.com/protocolbuffers/protobuf.git",
)

git_repository(
    name = "com_google_protobuf_cc",
    commit = "f425b9f",
#    commit = "6973c3a",  # release v3.7.1
    remote = "https://github.com/protocolbuffers/protobuf.git",
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
# Load common dependencies.
protobuf_deps()

git_repository(
    name = "com_google_absl",
    commit = "bf29470",
    remote = "https://github.com/abseil/abseil-cpp.git",
)

http_archive(
    name = "gtest",
    build_file = "//bazel:gtest.BUILD",
    strip_prefix = "googletest-release-1.8.0/googletest",
    url = "https://github.com/google/googletest/archive/release-1.8.0.zip",
)

http_archive(
    name = "glpk",
    build_file = "//bazel:glpk.BUILD",
    sha256 = "9a5dab356268b4f177c33e00ddf8164496dc2434e83bd1114147024df983a3bb",
    url = "http://ftp.gnu.org/gnu/glpk/glpk-4.52.tar.gz",
)
