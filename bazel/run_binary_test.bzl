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

"""run_binary_test will run a xx_binary as test with the given args.
"""

load("@bazel_skylib//rules:expand_template.bzl", "expand_template")
load("@rules_shell//shell:sh_test.bzl", "sh_test")

def parse_label(label):
    """Parse a label into (package, name).

    Args:
      label: string in relative or absolute form.

    Returns:
      Pair of strings: package, relative_name

    Raises:
      ValueError for malformed label (does not do an exhaustive validation)
    """
    if label.startswith("//"):
        label = label[2:]  # drop the leading //
        colon_split = label.split(":")
        if len(colon_split) == 1:  # no ":" in label
            pkg = label
            _, _, target = label.rpartition("/")
        else:
            pkg, target = colon_split  # fails if len(colon_split) != 2
    else:
        colon_split = label.split(":")
        if len(colon_split) == 1:  # no ":" in label
            pkg, target = native.package_name(), label
        else:
            pkg2, target = colon_split  # fails if len(colon_split) != 2
            pkg = native.package_name() + ("/" + pkg2 if pkg2 else "")
    return pkg, target

def run_binary_test(
        name,
        binary,
        template = "//bazel:test_runner_template",
        args = [],
        data = [],
        **kwargs):
    """Create a sh_test to run the given binary as test.

    Args:
      name: name of the test target.
      binary: name of the binary target to run.
      template: template file for executing the binary target.
      args: args to use to run the binary.
      data: data files required by this test.
      **kwargs: other attributes that are applicable to tests, size, tags, etc.

    """
    shell_script = name + ".sh"

    # Get the path to the binary we want to run.
    binary_pkg, binary_name = parse_label(binary)
    binary_path = "/".join([binary_pkg, binary_name])

    # We would like to include args in the generated shell script, so that "blaze-bin/.../test" can
    # be run manually. Unfortunately `expand_template` does not resolve $(location) and other Make
    # variables so we only pass them in `sh_test` below.
    expand_template(
        name = name + "_gensh",
        template = template,
        out = shell_script,
        testonly = 1,
        substitutions = {
            "{package_name}": native.package_name(),
            "{target}": name,
            "{binary_path}": binary_path,
        },
    )
    sh_test(
        name = name,
        testonly = 1,
        srcs = [shell_script],
        data = data + [binary],
        args = args,
        **kwargs
    )
