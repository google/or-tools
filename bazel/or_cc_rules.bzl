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

"""Wrapper rules around cc_library, cc_binary, and cc_test for OR-Tools.

These wrappers automatically prepend ORTOOLS_DEFAULT_COPTS (or ORTOOLS_TEST_COPTS
for tests) to `copts` and ORTOOLS_DEFAULT_LINKOPTS to `linkopts`, ensuring
consistent compiler and linker flags across all exported C++ targets.
"""

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("//ortools/copts:configure_copts.bzl", "ORTOOLS_DEFAULT_COPTS", "ORTOOLS_DEFAULT_LINKOPTS", "ORTOOLS_TEST_COPTS")

def or_cc_library(name, copts = [], linkopts = [], **kwargs):
    """Wrapper around cc_library that adds OR-Tools default copts and linkopts.

    Args:
      name: The name of the cc_library target.
      copts: Additional compiler options to append after ORTOOLS_DEFAULT_COPTS.
      linkopts: Additional linker options to append after ORTOOLS_DEFAULT_LINKOPTS.
      **kwargs: Additional keyword arguments forwarded to cc_library.
    """
    cc_library(
        name = name,
        copts = ORTOOLS_DEFAULT_COPTS + copts,
        linkopts = ORTOOLS_DEFAULT_LINKOPTS + linkopts,
        **kwargs
    )

def or_cc_binary(name, copts = [], linkopts = [], **kwargs):
    """Wrapper around cc_binary that adds OR-Tools default copts and linkopts.

    Args:
      name: The name of the cc_binary target.
      copts: Additional compiler options to append after ORTOOLS_DEFAULT_COPTS.
      linkopts: Additional linker options to append after ORTOOLS_DEFAULT_LINKOPTS.
      **kwargs: Additional keyword arguments forwarded to cc_binary.
    """
    cc_binary(
        name = name,
        copts = ORTOOLS_DEFAULT_COPTS + copts,
        linkopts = ORTOOLS_DEFAULT_LINKOPTS + linkopts,
        **kwargs
    )

def or_cc_test(name, copts = [], linkopts = [], **kwargs):
    """Wrapper around cc_test that adds OR-Tools test copts and default linkopts.

    Args:
      name: The name of the cc_test target.
      copts: Additional compiler options to append after ORTOOLS_TEST_COPTS.
      linkopts: Additional linker options to append after ORTOOLS_DEFAULT_LINKOPTS.
      **kwargs: Additional keyword arguments forwarded to cc_test.
    """
    cc_test(
        name = name,
        copts = ORTOOLS_TEST_COPTS + copts,
        linkopts = ORTOOLS_DEFAULT_LINKOPTS + linkopts,
        **kwargs
    )
