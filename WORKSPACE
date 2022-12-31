# Copyright 2010-2022 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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

# Abseil-cpp
git_repository(
    name = "com_google_absl",
    tag = "20220623.1",
    remote = "https://github.com/abseil/abseil-cpp.git",
)

# Protobuf
git_repository(
    name = "com_google_protobuf",
    tag = "v21.10",
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
    tag = "2022-04-01",
    remote = "https://github.com/google/re2.git",
)

git_repository(
    name = "com_google_googletest",
    tag = "release-1.12.1",
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
    tag = "v803",
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

git_repository(
    name = "highs",
    branch = "bazel",
    remote = "https://github.com/ERGO-Code/HiGHS.git",
)

# Python
## Bazel Python rules.
git_repository(
    name = "rules_python",
    tag = "0.12.0",
    remote = "https://github.com/bazelbuild/rules_python.git",
)

load("@rules_python//python:pip.bzl", "pip_install")

# Create a central external repo, @ortools_deps, that contains Bazel targets for all the
# third-party packages specified in the python_deps.txt file.
pip_install(
   name = "ortools_deps",
   requirements = "//bazel:python_deps.txt",
)

git_repository(
    name = "pybind11_bazel",
    commit = "faf56fb3df11287f26dbc66fdedf60a2fc2c6631",
    patches = ["//patches:pybind11_bazel.patch"],
    patch_args = ["-p1"],
    remote = "https://github.com/pybind/pybind11_bazel.git",
)

new_git_repository(
    name = "pybind11",
    build_file = "@pybind11_bazel//:pybind11.BUILD",
    tag = "v2.10.2",
    remote = "https://github.com/pybind/pybind11.git",
)

load("@pybind11_bazel//:python_configure.bzl", "python_configure")
python_configure(name = "local_config_python", python_version = "3")

# Swig support

# pcre source code repository
http_archive(
    name = "pcre2",
    build_file = "//bazel:pcre2.BUILD",
    patches = ["//bazel:pcre2.patch"],
    strip_prefix = "pcre2-10.42",
    urls = [
        "https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.42/pcre2-10.42.tar.gz",
    ],
    sha256 = "c33b418e3b936ee3153de2c61cc638e7e4fe3156022a5c77d0711bcbb9d64f1f",
)

# generate Patch:
#   Checkout swig
#   cd Source/CParse && bison -d -o parser.c parser.y
#   ./autogen.sh
#   ./configure
#   make Lib/swigwarn.swg
#   edit .gitignore and remove parser.h, parser.c, and swigwarn.swg
#   git add Source/CParse/parser.h Source/CParse/parser.c Lib/swigwarn.swg
#   git diff --staged Lib Source/CParse > <path to>swig.patch
# Edit swig.BUILD:
#   edit version
new_git_repository(
    name = "swig",
    build_file = "//bazel:swig.BUILD",
    patches = ["//bazel:swig.patch"],
    patch_args = ["-p1"],
    tag = "v4.1.1",
    remote = "https://github.com/swig/swig.git",
)

# Java support
load("@rules_jvm_external//:defs.bzl", "maven_install")

maven_install(
    artifacts = [
        "net.java.dev.jna:jna:aar:5.8.0"
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)


