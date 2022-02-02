workspace(name = "com_google_ortools")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")

# Bazel Skylib rules.
git_repository(
    name = "bazel_skylib",
    commit = "b2ed616",  # release 1.1.1
    remote = "https://github.com/bazelbuild/bazel-skylib.git",
)
load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
bazel_skylib_workspace()

# Bazel Platforms rules.
git_repository(
    name = "platforms",
    commit = "d4c9d7f",  # release 0.0.4
    remote = "https://github.com/bazelbuild/platforms.git",
)

# Bazel Python rules.
git_repository(
    name = "rules_python",
    commit = "b842276",  # release 0.6.0
    remote = "https://github.com/bazelbuild/rules_python.git",
)

# ZLIB
http_archive(
    name = "zlib",
    build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
    sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
    strip_prefix = "zlib-1.2.11",
    urls = [
        "https://mirror.bazel.build/zlib.net/zlib-1.2.11.tar.gz",
        "https://zlib.net/zlib-1.2.11.tar.gz",
    ],
)

# Protobuf
git_repository(
    name = "com_google_protobuf",
    commit = "22d0e26",  # release v3.19.4
    remote = "https://github.com/protocolbuffers/protobuf.git",
)
# Load common dependencies.
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
protobuf_deps()

git_repository(
    name = "com_google_absl",
    commit = "2151058", # release 20211102.0
    remote = "https://github.com/abseil/abseil-cpp.git",
)

git_repository(
    name = "com_google_re2",
    patches = ["//bazel:re2.patch"],
    commit = "0dade9f", # release 2021-11-01
    remote = "https://github.com/google/re2.git",
)

git_repository(
    name = "com_google_googletest",
    commit = "e2239ee", # release-1.11.0
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
    commit = "6acb7222e1b871041445bee75fc05bd1bcaed089", # master from Jul 19, 2021
    remote = "https://github.com/scipopt/scip.git",
)
