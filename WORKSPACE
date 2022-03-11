workspace(name = "com_google_ortools")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")

# Bazel Skylib rules.
git_repository(
    name = "bazel_skylib",
    tag = "1.2.1",
    remote = "https://github.com/bazelbuild/bazel-skylib.git",
)
load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
bazel_skylib_workspace()

# Bazel Platforms rules.
git_repository(
    name = "platforms",
    tag = "0.0.5",
    remote = "https://github.com/bazelbuild/platforms.git",
)

# Bazel Python rules.
git_repository(
    name = "rules_python",
    tag = "0.6.0",
    remote = "https://github.com/bazelbuild/rules_python.git",
)

# Abseil-cpp
git_repository(
    name = "com_google_absl",
    tag = "20211102.0",
    remote = "https://github.com/abseil/abseil-cpp.git",
)

# Protobuf
git_repository(
    name = "com_google_protobuf",
    tag = "v3.19.4",
    remote = "https://github.com/protocolbuffers/protobuf.git",
)
# Load common dependencies.
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
protobuf_deps()

# ZLIB
new_git_repository(
    name = "zlib",
    build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
    tag = "v1.2.11",
    remote = "https://github.com/madler/zlib.git",
)

git_repository(
    name = "com_google_re2",
    patches = ["//bazel:re2.patch"],
    tag = "2022-02-01",
    remote = "https://github.com/google/re2.git",
)

git_repository(
    name = "com_google_googletest",
    tag = "release-1.11.0",
    remote = "https://github.com/google/googletest.git",
)

http_archive(
    name = "glpk",
    build_file = "//bazel:glpk.BUILD",
    sha256 = "4a1013eebb50f728fc601bdd833b0b2870333c3b3e5a816eeba921d95bec6f15",
    url = "http://ftp.gnu.org/gnu/glpk/glpk-5.0.tar.gz",
)

http_archive(
    name = "bliss",
    build_file = "//bazel:bliss.BUILD",
    patches = ["//bazel:bliss-0.73.patch"],
    sha256 = "f57bf32804140cad58b1240b804e0dbd68f7e6bf67eba8e0c0fa3a62fd7f0f84",
    url = "https://github.com/google/or-tools/releases/download/v9.0/bliss-0.73.zip",
    #url = "http://www.tcs.hut.fi/Software/bliss/bliss-0.73.zip",
)

new_git_repository(
    name = "scip",
    build_file = "//bazel:scip.BUILD",
    patches = ["//bazel:scip.patch"],
    patch_args = ["-p1"],
    tag = "v800",
    remote = "https://github.com/scipopt/scip.git",
)

# Eigen has no Bazel build.
new_git_repository(
    name = "eigen",
    tag = "3.4.0",
    remote = "https://gitlab.com/libeigen/eigen.git",
    build_file_content =
"""
cc_library(
    name = 'eigen3',
    srcs = [],
    includes = ['.'],
    hdrs = glob(['Eigen/**']),
    visibility = ['//visibility:public'],
)
"""
)
