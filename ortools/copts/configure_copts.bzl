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

"""configure copts.

This file simply selects the correct options from the generated files
"""

# TODO(user): Autogenerate these lists to keep them in sync with CMake.

# clang-cl
ORTOOLS_CLANG_CL_FLAGS = []
ORTOOLS_CLANG_CL_TEST_FLAGS = []

# gcc
ORTOOLS_GCC_FLAGS = []
ORTOOLS_GCC_TEST_FLAGS = []

# llvm
ORTOOLS_LLVM_FLAGS = []
ORTOOLS_LLVM_TEST_FLAGS = []

# msvc
ORTOOLS_MSVC_FLAGS = []
ORTOOLS_MSVC_LINKOPTS = []
ORTOOLS_MSVC_TEST_FLAGS = []

ORTOOLS_DEFAULT_COPTS = select({
    "@rules_cc//cc/compiler:msvc-cl": ORTOOLS_MSVC_FLAGS,
    "@rules_cc//cc/compiler:clang-cl": ORTOOLS_CLANG_CL_FLAGS,
    "@rules_cc//cc/compiler:clang": ORTOOLS_LLVM_FLAGS,
    "@rules_cc//cc/compiler:gcc": ORTOOLS_GCC_FLAGS,
    "//conditions:default": [],
})

ORTOOLS_TEST_COPTS = select({
    "@rules_cc//cc/compiler:msvc-cl": ORTOOLS_MSVC_TEST_FLAGS,
    "@rules_cc//cc/compiler:clang-cl": ORTOOLS_CLANG_CL_TEST_FLAGS,
    "@rules_cc//cc/compiler:clang": ORTOOLS_LLVM_TEST_FLAGS,
    "@rules_cc//cc/compiler:gcc": ORTOOLS_GCC_TEST_FLAGS,
    "//conditions:default": [],
})

ORTOOLS_DEFAULT_LINKOPTS = select({
    "@rules_cc//cc/compiler:msvc-cl": ORTOOLS_MSVC_LINKOPTS,
    "//conditions:default": [],
})
