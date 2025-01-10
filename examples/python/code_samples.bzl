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
load("@rules_python//python:defs.bzl", "py_binary", "py_test")

PYTHON_DEPS = [
    "//ortools/init/python:init",
    "//ortools/linear_solver/python:model_builder",
    "//ortools/sat/python:cp_model",
    "//ortools/sat/colab:visualization",
    "//ortools/scheduling:rcpsp_py_proto",
    "//ortools/scheduling/python:rcpsp",
    requirement("absl-py"),
    requirement("numpy"),
    requirement("pandas"),
    requirement("protobuf"),
    requirement("python_dateutil"),
    requirement("pytz"),
    requirement("six"),
]

def code_sample_compile_py(name):
    py_binary(
        name = name + "_py3",
        srcs = [name + ".py"],
        main = name + ".py",
        deps = PYTHON_DEPS,
        python_version = "PY3",
        srcs_version = "PY3",
    )

def code_sample_test_py(name):
    py_test(
        name = name + "_py_test",
        size = "medium",
        srcs = [name + ".py"],
        main = name + ".py",
        deps = PYTHON_DEPS,
        python_version = "PY3",
        srcs_version = "PY3",
    )

def code_sample_test_arg_py(name, suffix, args, data):
    py_test(
        name = name + "_" + suffix + "_py_test",
        size = "medium",
        srcs = [name + ".py"],
        main = name + ".py",
        data = data,
        args = args,
        deps = PYTHON_DEPS,
        python_version = "PY3",
        srcs_version = "PY3",
    )

def code_sample_py(name):
    code_sample_compile_py(name)
    code_sample_test_py(name)
