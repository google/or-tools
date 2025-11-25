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

"""Macros to test binaries."""

load("@pip_deps//:requirements.bzl", "requirement")
load("@rules_python//python:py_test.bzl", "py_test")

def _get_path(label):
    """Returns the path of a label."""

    # According to https://bazel.build/reference/be/make-variables#predefined_label_variables
    # `rootpath` may fail to work for non C++ binaries. As an alternative if such an issue comes
    # up we can create the absolute path manually from the label.
    return "$(rootpath %s)" % label

def _get_env(named_data):
    """Returns the environment variables to pass to the test."""
    return {
        "BINTEST_" + key: _get_path(label)
        for key, label in named_data.items()
    }

def py_bintest(named_data, env = {}, deps = [], **kwargs):
    """Runs a Python test that can access data binaries.

    Args:
        named_data: A dictionary mapping names to data labels. The test will be
            able to access the path of each data label using the `$(<label>)` construct.
        env: A dictionary of environment variables to pass to the test.
        deps: A list of dependencies for the test.
        **kwargs: Additional arguments to pass to `py_test`.
    """
    py_test(
        data = named_data.values(),
        env = env | _get_env(named_data),
        deps = deps + [
            "//tools/testing:binary_test",
            "//tools/testing:bintest_matchers",
            "//tools/testing:bintest_run_utils",
        ],
        **kwargs
    )

def bintest(name, srcs, named_data = {}, **kwargs):
    """Runs a script and checks its output.

    The script is run by `bintest_script_runner.py`.

    Args:
        name: The name of the test.
        srcs: A list containing a single element, the label of the script to run.
        named_data: A dictionary mapping names to data labels. The test will be
            able to access the path of each data label using the `$(<label>)` construct.
        **kwargs: Additional arguments to pass to `py_test`.
    """
    if type(srcs) != type([]) or len(srcs) != 1:
        fail("srcs must be a list containin a single element")
    script = srcs[0]
    script_path = _get_path(script)
    py_test(
        name = name,
        srcs = [
            "//tools/testing:bintest_script_launcher.py"
        ],
        main = "bintest_script_launcher.py",
        args = [script_path],
        data = srcs + named_data.values(),
        env = _get_env(named_data),
        deps = [
            requirement("absl-py"),
            "//tools/testing:bintest_script_runner",
        ],
        **kwargs
    )
