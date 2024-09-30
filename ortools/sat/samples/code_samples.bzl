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

"""Helper macro to compile and test code samples."""

load("@io_bazel_rules_go//go:def.bzl", "go_test")
load("@pip_deps//:requirements.bzl", "requirement")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_test")
load("@rules_java//java:defs.bzl", "java_test")
load("@rules_python//python:defs.bzl", "py_binary", "py_test")

def code_sample_cc(name):
    cc_binary(
        name = name + "_cc",
        srcs = [name + ".cc"],
        deps = [
            "//ortools/sat:cp_model",
            "//ortools/sat:cp_model_solver",
            "//ortools/util:sorted_interval_list",
            "@com_google_absl//absl/types:span",
        ],
    )

    cc_test(
        name = name + "_cc_test",
        size = "small",
        srcs = [name + ".cc"],
        deps = [
            ":" + name + "_cc",
            "//ortools/sat:cp_model",
            "//ortools/sat:cp_model_solver",
            "//ortools/util:sorted_interval_list",
            "@com_google_absl//absl/types:span",
        ],
    )

def code_sample_go(name):
    go_test(
        name = name + "_go_test",
        size = "small",
        srcs = [name + ".go"],
        deps = [
            "//ortools/sat:cp_model_go_proto",
            "//ortools/sat:sat_parameters_go_proto",
            "//ortools/sat/go/cpmodel",
            "@com_github_golang_glog//:glog",
            "@org_golang_google_protobuf//proto",
        ],
    )

def code_sample_py(name):
    py_binary(
        name = name + "_py3",
        srcs = [name + ".py"],
        main = name + ".py",
        deps = [
            requirement("absl-py"),
            requirement("numpy"),
            requirement("pandas"),
            "//ortools/sat/python:cp_model",
        ],
        python_version = "PY3",
        srcs_version = "PY3",
    )

    py_test(
        name = name + "_py_test",
        size = "small",
        srcs = [name + ".py"],
        main = name + ".py",
        data = [
            "//ortools/sat/python:cp_model",
        ],
        deps = [
            requirement("absl-py"),
            requirement("numpy"),
            requirement("pandas"),
            requirement("protobuf"),
        ],
        python_version = "PY3",
        srcs_version = "PY3",
    )

def code_sample_cc_go_py(name):
    code_sample_cc(name = name)
    code_sample_go(name = name)
    code_sample_py(name = name)

def code_sample_cc_py(name):
    code_sample_cc(name = name)
    code_sample_py(name = name)

def code_sample_java(name):
    java_test(
        name = name + "_java_test",
        size = "small",
        srcs = [name + ".java"],
        main_class = "com.google.ortools.sat.samples." + name,
        test_class = "com.google.ortools.sat.samples." + name,
        deps = [
            "//ortools/sat/java:sat",
            "//ortools/java/com/google/ortools:Loader",
            "//ortools/java/com/google/ortools/sat:sat",
            "//ortools/sat:cp_model_java_proto",
            "//ortools/sat:sat_parameters_java_proto",
            "//ortools/util/java:sorted_interval_list",
        ],
    )
