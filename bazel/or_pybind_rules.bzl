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

"""Wrapper rule around pybind_extension for OR-Tools.

This wrapper automatically prepends ORTOOLS_DEFAULT_COPTS to `copts` and
ORTOOLS_DEFAULT_LINKOPTS to `linkopts`, ensuring consistent compiler and linker
flags across all exported pybind11 extension targets.
"""

load("//ortools/copts:configure_copts.bzl", "ORTOOLS_DEFAULT_COPTS", "ORTOOLS_DEFAULT_LINKOPTS")
load("@pybind11_bazel//:build_defs.bzl", "pybind_extension", "pybind_library")

def or_pybind_extension(name, copts = [], linkopts = [], **kwargs):
    """Wrapper around pybind_extension that adds OR-Tools default copts and linkopts.

    Args:
      name: The name of the pybind_extension target.
      copts: Additional compiler options to append after ORTOOLS_DEFAULT_COPTS.
      linkopts: Additional linker options to append after ORTOOLS_DEFAULT_LINKOPTS.
      **kwargs: Additional keyword arguments forwarded to pybind_extension.
    """
    pybind_extension(
        name = name,
        copts = ORTOOLS_DEFAULT_COPTS + copts,
        linkopts = ORTOOLS_DEFAULT_LINKOPTS + linkopts,
        **kwargs
    )

def or_pybind_library(name, copts = [], linkopts = [], **kwargs):
    """Wrapper around pybind_library that adds OR-Tools default copts and linkopts.

    Args:
      name: The name of the pybind_library target.
      copts: Additional compiler options to append after ORTOOLS_DEFAULT_COPTS.
      linkopts: Additional linker options to append after ORTOOLS_DEFAULT_LINKOPTS.
      **kwargs: Additional keyword arguments forwarded to pybind_library.
    """
    pybind_library(
        name = name,
        copts = ORTOOLS_DEFAULT_COPTS + copts,
        linkopts = ORTOOLS_DEFAULT_LINKOPTS + linkopts,
        **kwargs
    )
