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

"""Helper macro to compile and test code samples."""

load("@ortools_deps//:requirements.bzl", "requirement")

def code_sample_cc(name):
    native.cc_binary(
        name = name + "_cc",
        srcs = [name + ".cc"],
        deps = [
            "//ortools/base",
            "//ortools/graph:assignment",
            "//ortools/graph:ebert_graph",
            "//ortools/graph:linear_assignment",
            "//ortools/graph:max_flow",
            "//ortools/graph:min_cost_flow",
            "//ortools/graph:shortestpaths",
        ],
    )

    native.cc_test(
        name = name + "_cc_test",
        size = "small",
        srcs = [name + ".cc"],
        deps = [
            ":" + name + "_cc",
            "//ortools/base",
            "//ortools/graph:assignment",
            "//ortools/graph:ebert_graph",
            "//ortools/graph:linear_assignment",
            "//ortools/graph:max_flow",
            "//ortools/graph:min_cost_flow",
            "//ortools/graph:shortestpaths",
        ],
    )

def code_sample_py(name):
    native.py_binary(
        name = name + "_py3",
        srcs = [name + ".py"],
        main = name + ".py",
        data = [
            "//ortools/graph/python:linear_sum_assignment.so",
            "//ortools/graph/python:min_cost_flow.so",
            "//ortools/graph/python:max_flow.so",
        ],
        deps = [
            requirement("absl-py"),
            requirement("numpy"),
        ],
        python_version = "PY3",
        srcs_version = "PY3",
    )

    native.py_test(
        name = name + "_py_test",
        size = "small",
        srcs = [name + ".py"],
        main = name + ".py",
        data = [
            "//ortools/graph/python:linear_sum_assignment.so",
            "//ortools/graph/python:min_cost_flow.so",
            "//ortools/graph/python:max_flow.so",
        ],
        deps = [
            requirement("absl-py"),
            requirement("numpy"),
        ],
        python_version = "PY3",
        srcs_version = "PY3",
    )

def code_sample_cc_py(name):
    code_sample_cc(name = name)
    code_sample_py(name = name)
