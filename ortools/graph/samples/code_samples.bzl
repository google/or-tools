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
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_test")
load("@rules_java//java:defs.bzl", "java_test")
load("@rules_python//python:defs.bzl", "py_binary", "py_test")

def code_sample_cc(name):
    cc_binary(
        name = name + "_cc",
        srcs = [name + ".cc"],
        deps = [
            "//ortools/base",
            "//ortools/base:status_macros",
            "//ortools/base:top_n",
            "//ortools/graph:assignment",
            "//ortools/graph:bounded_dijkstra",
            "//ortools/graph:bfs",
            "//ortools/graph:dag_constrained_shortest_path",
            "//ortools/graph:dag_shortest_path",
            "//ortools/graph:linear_assignment",
            "//ortools/graph:max_flow",
            "//ortools/graph:min_cost_flow",
            "//ortools/graph:rooted_tree",
            "@com_google_absl//absl/random",
        ],
    )

    cc_test(
        name = name + "_cc_test",
        size = "small",
        srcs = [name + ".cc"],
        deps = [
            ":" + name + "_cc",
            "//ortools/base",
            "//ortools/base:status_macros",
            "//ortools/base:top_n",
            "//ortools/graph:assignment",
            "//ortools/graph:bounded_dijkstra",
            "//ortools/graph:bfs",
            "//ortools/graph:dag_constrained_shortest_path",
            "//ortools/graph:dag_shortest_path",
            "//ortools/graph:linear_assignment",
            "//ortools/graph:max_flow",
            "//ortools/graph:min_cost_flow",
            "//ortools/graph:rooted_tree",
            "@com_google_absl//absl/random",
        ],
    )

def code_sample_py(name):
    py_binary(
        name = name + "_py3",
        srcs = [name + ".py"],
        main = name + ".py",
        deps = [
            "//ortools/graph/python:linear_sum_assignment",
            "//ortools/graph/python:min_cost_flow",
            "//ortools/graph/python:max_flow",
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
            "//ortools/graph/python:linear_sum_assignment",
            "//ortools/graph/python:min_cost_flow",
            "//ortools/graph/python:max_flow",
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
    java_test(
        name = name + "_java_test",
        size = "small",
        srcs = [name + ".java"],
        main_class = "com.google.ortools.graph.samples." + name,
        test_class = "com.google.ortools.graph.samples." + name,
        deps = [
            "//ortools/graph/java:graph",
            "//ortools/java/com/google/ortools:Loader",
        ],
    )
