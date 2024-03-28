# Copyright 2010-2024 Google LLC
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

# Bazel Extensions
## Bazel Skylib rules.
git_repository(
    name = "bazel_skylib",
    tag = "1.5.0",
    remote = "https://github.com/bazelbuild/bazel-skylib.git",
)
load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
bazel_skylib_workspace()

## Bazel rules.
git_repository(
    name = "platforms",
    tag = "0.0.8",
    remote = "https://github.com/bazelbuild/platforms.git",
)

git_repository(
    name = "rules_cc",
    tag = "0.0.9",
    remote = "https://github.com/bazelbuild/rules_cc.git",
)

git_repository(
    name = "rules_proto",
    tag = "5.3.0-21.7",
    remote = "https://github.com/bazelbuild/rules_proto.git",
)

git_repository(
    name = "rules_java",
    tag = "7.3.2",
    remote = "https://github.com/bazelbuild/rules_java.git",
)

git_repository(
    name = "rules_jvm_external",
    tag = "6.0",
    remote = "https://github.com/bazelbuild/rules_jvm_external.git",
)

git_repository(
    name = "contrib_rules_jvm",
    tag = "v0.24.0",
    remote = "https://github.com/bazel-contrib/rules_jvm.git",
)

git_repository(
    name = "rules_python",
    tag = "0.29.0",
    remote = "https://github.com/bazelbuild/rules_python.git",
)

# Dependencies
## ZLIB
new_git_repository(
    name = "zlib",
    build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
    tag = "v1.2.13",
    remote = "https://github.com/madler/zlib.git",
)

## Abseil-cpp
git_repository(
    name = "com_google_absl",
    tag = "20240116.1",
    patches = ["//patches:abseil-cpp-20240116.1.patch"],
    patch_args = ["-p1"],
    remote = "https://github.com/abseil/abseil-cpp.git",
)

## Re2
git_repository(
    name = "com_google_re2",
    tag = "2024-03-01",
    remote = "https://github.com/google/re2.git",
    repo_mapping = {"@abseil-cpp": "@com_google_absl"},
)

## Protobuf
# proto_library, cc_proto_library, and java_proto_library rules implicitly
# depend on @com_google_protobuf for protoc and proto runtimes.
# This statement defines the @com_google_protobuf repo.
git_repository(
    name = "com_google_protobuf",
    patches = ["//patches:protobuf-v26.1.patch"],
    patch_args = ["-p1"],
    tag = "v26.1",
    remote = "https://github.com/protocolbuffers/protobuf.git",
)
# Load common dependencies.
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
protobuf_deps()

## Solvers
http_archive(
    name = "glpk",
    build_file = "//bazel:glpk.BUILD.bazel",
    sha256 = "4a1013eebb50f728fc601bdd833b0b2870333c3b3e5a816eeba921d95bec6f15",
    url = "http://ftp.gnu.org/gnu/glpk/glpk-5.0.tar.gz",
)

http_archive(
    name = "bliss",
    build_file = "//bazel:bliss.BUILD.bazel",
    patches = ["//bazel:bliss-0.73.patch"],
    sha256 = "f57bf32804140cad58b1240b804e0dbd68f7e6bf67eba8e0c0fa3a62fd7f0f84",
    url = "https://github.com/google/or-tools/releases/download/v9.0/bliss-0.73.zip",
    #url = "http://www.tcs.hut.fi/Software/bliss/bliss-0.73.zip",
)

new_git_repository(
    name = "scip",
    build_file = "//bazel:scip.BUILD.bazel",
    patches = ["//bazel:scip-v900.patch"],
    patch_args = ["-p1"],
    tag = "v900",
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
    defines = ["EIGEN_MPL2_ONLY",],
    visibility = ['//visibility:public'],
)
"""
)

git_repository(
    name = "highs",
    branch = "bazel",
    remote = "https://github.com/ERGO-Code/HiGHS.git",
)

## Swig support
# pcre source code repository
new_git_repository(
    name = "pcre2",
    build_file = "//bazel:pcre2.BUILD.bazel",
    tag = "pcre2-10.42",
    remote = "https://github.com/PCRE2Project/pcre2.git",
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
# Edit swig.BUILD.bazel:
#   edit version
new_git_repository(
    name = "swig",
    build_file = "//bazel:swig.BUILD.bazel",
    patches = ["//bazel:swig.patch"],
    patch_args = ["-p1"],
    tag = "v4.2.0",
    remote = "https://github.com/swig/swig.git",
)

## Python
load("@rules_python//python:repositories.bzl", "py_repositories")
py_repositories()

# Create a central external repo, @pip_deps, that contains Bazel targets for all the
# third-party packages specified in the bazel/requirements.txt file.
load("@rules_python//python:pip.bzl", "pip_parse")
pip_parse(
   name = "pip_deps",
   requirements_lock = "//bazel:ortools_requirements.txt",
)

load("@pip_deps//:requirements.bzl",
     install_pip_deps="install_deps")
install_pip_deps()

# Add a second repo @ortools_notebook_deps for jupyter notebooks.
pip_parse(
   name = "ortools_notebook_deps",
   requirements_lock = "//bazel:notebook_requirements.txt",
)

load("@ortools_notebook_deps//:requirements.bzl",
     install_notebook_deps="install_deps")
install_notebook_deps()

# Protobuf
load("@com_google_protobuf//bazel:system_python.bzl", "system_python")
system_python(
    name = "system_python",
    minimum_python_version = "3.8",
)

# Absl python library
http_archive(
    name = "com_google_absl_py",
    repo_mapping = {"@six_archive": "@six"},
    sha256 = "0be59b82d65dfa1f995365dcfea2cc57989297b065fda696ef13f30fcc6c8e5b",
    strip_prefix = "abseil-py-pypi-v0.15.0",
    urls = [
        "https://github.com/abseil/abseil-py/archive/refs/tags/pypi-v0.15.0.tar.gz",
    ],
)

## `pybind11_bazel`
git_repository(
    name = "pybind11_bazel",
    commit = "23926b00e2b2eb2fc46b17e587cf0c0cfd2f2c4b", # 2023/11/29
    patches = ["//patches:pybind11_bazel.patch"],
    patch_args = ["-p1"],
    remote = "https://github.com/pybind/pybind11_bazel.git",
)

new_git_repository(
    name = "pybind11",
    build_file = "@pybind11_bazel//:pybind11.BUILD",
    tag = "v2.11.1",
    remote = "https://github.com/pybind/pybind11.git",
)

new_git_repository(
    name = "pybind11_abseil",
    commit = "52f27398876a3177049977249e004770bd869e61", # 2024/01/11
    patches = ["//patches:pybind11_abseil.patch"],
    patch_args = ["-p1"],
    remote = "https://github.com/pybind/pybind11_abseil.git",
)

new_git_repository(
    name = "pybind11_protobuf",
    commit = "3b11990a99dea5101799e61d98a82c4737d240cc", # 2024/01/04
    remote = "https://github.com/pybind/pybind11_protobuf.git",
)

load("@pybind11_bazel//:python_configure.bzl", "python_configure")
python_configure(name = "local_config_python", python_version = "3")
bind(
    name = "python_headers",
    actual = "@local_config_python//:python_headers",
)

## Java support (with junit 5)
load("@rules_java//java:repositories.bzl", "rules_java_dependencies", "rules_java_toolchains")
rules_java_dependencies()
rules_java_toolchains()

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")
rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")
rules_jvm_external_setup()

JUNIT_PLATFORM_VERSION = "1.9.2"
JUNIT_JUPITER_VERSION = "5.9.2"
load("@rules_jvm_external//:defs.bzl", "maven_install")
maven_install(
    artifacts = [
        "net.java.dev.jna:jna:5.14.0",
        "com.google.truth:truth:0.32",
        "org.junit.platform:junit-platform-launcher:%s" % JUNIT_PLATFORM_VERSION,
        "org.junit.platform:junit-platform-reporting:%s" % JUNIT_PLATFORM_VERSION,
        "org.junit.jupiter:junit-jupiter-api:%s" % JUNIT_JUPITER_VERSION,
        "org.junit.jupiter:junit-jupiter-params:%s" % JUNIT_JUPITER_VERSION,
        "org.junit.jupiter:junit-jupiter-engine:%s" % JUNIT_JUPITER_VERSION,
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

load("@contrib_rules_jvm//:repositories.bzl", "contrib_rules_jvm_deps")
contrib_rules_jvm_deps()

load("@contrib_rules_jvm//:setup.bzl", "contrib_rules_jvm_setup")
contrib_rules_jvm_setup()

## Testing
git_repository(
    name = "com_google_googletest",
    tag = "v1.13.0",
    remote = "https://github.com/google/googletest.git",
)

git_repository(
    name = "com_google_benchmark",
    tag = "v1.8.3",
    remote = "https://github.com/google/benchmark.git",
)
