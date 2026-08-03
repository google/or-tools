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

"""A wrapper around java_wrap_cc."""

load("//bazel:swig_java.bzl", "java_wrap_cc")

def or_java_wrap_cc(name, deps = [], java_deps = [], **kwargs):
    """
    A wrapper around java_wrap_cc that combines deps and java_deps.
    """

    java_wrap_cc(
        name = name,
        deps = deps,
        java_deps = java_deps,
        **kwargs
    )
