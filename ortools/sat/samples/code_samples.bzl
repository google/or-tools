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
            "//ortools/sat:cp_model",
            "//ortools/sat:cp_model_solver",
            "//ortools/util:sorted_interval_list",
            "@com_google_absl//absl/types:span",
        ],
    )

    native.cc_test(
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

def code_sample_py(name):
    native.py_binary(
        name = name + "_py3",
        srcs = [name + ".py"],
        main = name + ".py",
        deps = [
            requirement("absl-py"),
            "//ortools/sat/python:cp_model",
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
            "//ortools/sat/python:cp_model",
        ],
        deps = [
            requirement("absl-py"),
            requirement("numpy"),
            requirement("protobuf"),
        ],
        python_version = "PY3",
        srcs_version = "PY3",
    )

def code_sample_cc_py(name):
    code_sample_cc(name = name)
    code_sample_py(name = name)
