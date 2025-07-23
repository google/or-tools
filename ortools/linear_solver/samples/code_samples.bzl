# Copyright 2010-2025 Google LLC
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

load("@pip_deps//:requirements.bzl", "requirement")
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("@rules_java//java:defs.bzl", "java_test")
load("@rules_python//python:py_binary.bzl", "py_binary")
load("@rules_python//python:py_test.bzl", "py_test")

def code_sample_cc(name):
    cc_binary(
        name = name + "_cc",
        srcs = [name + ".cc"],
        deps = [
            "//ortools/base",
            "//ortools/init",
            "//ortools/linear_solver",
            "//ortools/linear_solver:linear_solver_cc_proto",
        ],
    )

    cc_test(
        name = name + "_cc_test",
        size = "small",
        srcs = [name + ".cc"],
        deps = [
            ":" + name + "_cc",
            "//ortools/base",
            "//ortools/init",
            "//ortools/linear_solver",
            "//ortools/linear_solver:linear_solver_cc_proto",
        ],
    )

def code_sample_py(name):
    py_binary(
        name = name + "_py3",
        srcs = [name + ".py"],
        main = name + ".py",
        deps = [
            requirement("absl-py"),
            requirement("protobuf"),
            requirement("numpy"),
            requirement("pandas"),
            "//ortools/init/python:init",
            "//ortools/linear_solver/python:model_builder",
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
            "//ortools/init/python:init",
            "//ortools/linear_solver/python:model_builder",
        ],
        deps = [
            requirement("absl-py"),
            requirement("protobuf"),
            requirement("numpy"),
            requirement("pandas"),
        ],
        python_version = "PY3",
        srcs_version = "PY3",
    )

def code_sample_java(name):
    java_test(
        name = name + "_java_test",
        size = "small",
        srcs = [name + ".java"],
        main_class = "com.google.ortools.linearsolver.samples." + name,
        test_class = "com.google.ortools.linearsolver.samples." + name,
        deps = [
            "//ortools/init/java:init",
            "//ortools/linear_solver/java:modelbuilder",
            "//ortools/java/com/google/ortools/modelbuilder",
            "//ortools/java/com/google/ortools:Loader",
        ],
    )
