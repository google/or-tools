load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")

#Import the gflags files.
git_repository(
    name = "com_github_gflags_gflags",
    commit = "f8a0efe03aa69b3336d8e228b37d4ccb17324b88",
    remote = "https://github.com/gflags/gflags.git",
)

#Import the glog files.
new_git_repository(
    name = "com_github_glog_glog",
    build_file = "//bazel:glog.BUILD",
    remote = "https://github.com/google/glog.git",
    tag = "v0.3.5",
)

http_archive(
    name = "com_google_protobuf",
    sha256 = "9510dd2afc29e7245e9e884336f848c8a6600a14ae726adb6befdb4f786f0be2",
    strip_prefix = "protobuf-3.6.1.3",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.6.1.3.zip"],
)

http_archive(
    name = "com_google_protobuf_cc",
    sha256 = "9510dd2afc29e7245e9e884336f848c8a6600a14ae726adb6befdb4f786f0be2",
    strip_prefix = "protobuf-3.6.1.3",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.6.1.3.zip"],
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
