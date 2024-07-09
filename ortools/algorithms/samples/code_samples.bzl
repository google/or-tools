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

load("@pip_deps//:requirements.bzl", "requirement")
load("@rules_python//python:defs.bzl", "py_binary", "py_test")

def code_sample_cc(name):
    native.cc_binary(
        name = name + "_cc",
        srcs = [name + ".cc"],
        deps = [
            "//ortools/algorithms:knapsack_solver_lib",
        ],
    )

    native.cc_test(
        name = name + "_cc_test",
        size = "small",
        srcs = [name + ".cc"],
        deps = [
            ":" + name + "_cc",
            "//ortools/algorithms:knapsack_solver_lib",
        ],
    )

def code_sample_py(name):
    py_binary(
        name = name + "_py3",
        srcs = [name + ".py"],
        main = name + ".py",
        deps = [
            "//ortools/algorithms/python:knapsack_solver",
            requirement("absl-py"),
            requirement("numpy"),
        ],
        python_version = "PY3",
        srcs_version = "PY3",
    )

    py_test(
        name = name + "_py_test",
        size = "small",
        srcs = [name + ".py"],
        main = name + ".py",
        deps = [
            "//ortools/algorithms/python:knapsack_solver",
            requirement("absl-py"),
            requirement("numpy"),
        ],
        python_version = "PY3",
        srcs_version = "PY3",
    )

def code_sample_cc_py(name):
    code_sample_cc(name = name)
    code_sample_py(name = name)

def code_sample_java(name):
    native.java_test(
        name = name + "_java_test",
        size = "small",
        srcs = [name + ".java"],
        main_class = "com.google.ortools.algorithms.samples." + name,
        test_class = "com.google.ortools.algorithms.samples." + name,
        deps = [
            "//ortools/algorithms/java:knapsacksolver",
            "//ortools/java/com/google/ortools:Loader",
        ],
    )
