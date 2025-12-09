#!/usr/bin/env python3
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

"""Converts bazel BUILD files to CMakeBazel.txt snippets.

This script processes BUILD.bazel files and generates CMakeBazel.txt files
containing CMake commands that mirror the bazel build rules.

Starlark is a subset of Python which allows "executing" them as Python code.
This script "executes" the BUILD.bazel file but provides different definitions
for the target we wants to export to CMake. These new definitions are
responsible for exporting writing the CMake equivalent of Bazel commands.
"""

from collections.abc import Sequence
import dataclasses
import os

from absl import app

# The following global variables are used to interact with the "exec" call.
CURRENT_CMAKE_PIECES = None  # The generated cmake is appended to this list.
ROOT_FOLDER = os.getcwd()  # The project root.
CURRENT_FOLDER = None  # The folder of currently processed BUILD.bazel file.
CURRENT_TARGET_PREFIX = None  # The prefix to use for exported targets.


@dataclasses.dataclass
class Label:
    """Helper class to manipulate bazel labels."""

    path: str
    root: str
    cmake_root: str

    def __init__(self, label: str):
        """Creates a Label object from a string."""
        if label.startswith("//"):
            self.path = label[2:].replace(":", "/")
            self.root = ROOT_FOLDER
            self.cmake_root = "${CMAKE_SOURCE_DIR}"
        elif label.startswith(":"):
            self.path = label[1:]
            self.root = CURRENT_FOLDER
            self.cmake_root = "${CMAKE_CURRENT_SOURCE_DIR}"
        else:
            assert not label.startswith("/")
            self.path = label
            self.root = CURRENT_FOLDER
            self.cmake_root = "${CMAKE_CURRENT_SOURCE_DIR}"

    def is_file(self) -> bool:
        """Returns true if the label is a file."""
        return os.path.isfile(os.path.join(self.root, self.path))

    def is_target(self) -> bool:
        """Returns true if the label is a target."""
        return not self.is_file()

    def as_cmake_target(self) -> str:
        """Returns the label as a cmake target."""
        assert self.is_target()
        return f"$<TARGET_FILE:{self.as_target_name()}>"

    def as_cmake_file(self) -> str:
        """Returns the label as a cmake file."""
        assert self.is_file()
        return os.path.join(self.cmake_root, self.path)

    def as_cmake(self) -> str:
        """Returns the label as a cmake string."""
        return self.as_cmake_file() if self.is_file() else self.as_cmake_target()

    def as_target_name(self) -> str:
        """Returns the label as a target name."""
        assert self.is_target()
        return CURRENT_TARGET_PREFIX + self.path


@dataclasses.dataclass
class Attr:
    """Helper class to manipulate cmake attributes."""

    name: str
    values: list[str]

    def __init__(self, name: str, *values: str):
        """Creates an Attr object from a name and a list of values."""
        self.name = name
        self.values = values

    def __str__(self):
        return f"  {self.name} {" ".join(self.values)}"

    def __bool__(self):
        return bool(self.values)


def name_attr(name: str) -> Attr:
    """Returns a NAME attribute."""
    return Attr("NAME", Label(name).as_target_name())


def sources_attr(srcs: Sequence[str], hdrs: Sequence[str]) -> Attr:
    """Returns a SOURCES attribute."""
    values = sorted(srcs + hdrs)
    return Attr("SOURCES", *values)


def link_libraries_attr(deps: Sequence[str]) -> Attr:
    """Returns a LINK_LIBRARIES attribute."""
    values = []
    for dep in deps:
        if not dep.startswith(":"):
            continue
        label = Label(dep)
        if label.is_target():
            values.append(label.as_target_name())
    return Attr("LINK_LIBRARIES", *values)


def type_attr(value: str) -> Attr:
    """Returns a TYPE attribute."""
    return Attr("TYPE", value)


def env_attr(named_data: dict[str, str]) -> Attr:
    """Returns an ENVIRONMENT attribute."""
    values = []
    for key, target in named_data.items():
        label = Label(target)
        values.append(f"BINTEST_{key}={label.as_cmake()}")
    return Attr("ENVIRONMENT", *values)


def script_attr(script: str) -> Attr:
    """Returns a SCRIPT attribute."""
    label = Label(script)
    assert label.is_file()
    return Attr("SCRIPT", label.as_cmake_file())


def add_call(call: str, attrs: Sequence[Attr]) -> str:
    """Adds a cmake call to the current cmake pieces."""
    CURRENT_CMAKE_PIECES.append(
        f"""{call}(
{'\n'.join(str(a) for a in filter(None, attrs))}
)"""
    )


# The functions below are the one replacing the bazel functions.


def cc_library(
    name, srcs=[], hdrs=[], deps=[], **kwargs
):  # pylint: disable=dangerous-default-value
    """Adds a cc_library to the current cmake pieces."""
    del kwargs
    add_call(
        "ortools_cxx_library",
        [
            name_attr(name),
            sources_attr(srcs, hdrs),
            link_libraries_attr(deps),
            type_attr("INTERFACE" if not srcs else "SHARED"),
        ],
    )


def cc_test(
    name, srcs=[], hdrs=[], deps=[], **kwargs
):  # pylint: disable=dangerous-default-value
    """Adds a cc_test to the current cmake pieces."""
    del kwargs
    add_call(
        "ortools_cxx_test",
        [name_attr(name), sources_attr(srcs, hdrs), link_libraries_attr(deps)],
    )


def cc_binary(
    name, srcs=[], hdrs=[], deps=[], **kwargs
):  # pylint: disable=dangerous-default-value
    """Adds a cc_binary to the current cmake pieces."""
    del kwargs
    add_call(
        "ortools_cxx_binary",
        [name_attr(name), sources_attr(srcs, hdrs), link_libraries_attr(deps)],
    )


def bintest(
    name, srcs=[], named_data={}, **kwargs
):  # pylint: disable=dangerous-default-value
    """Adds a bintest to the current cmake pieces."""
    del kwargs
    add_call(
        "ortools_cxx_bintest",
        [name_attr(name), script_attr(srcs[0]), env_attr(named_data)],
    )


# The functions above are the only one accessible when executing the bazel file.
EXEC_GLOBALS = {
    "bintest": bintest,
    "cc_binary": cc_binary,
    "cc_library": cc_library,
    "cc_test": cc_test,
} | {
    # The function below are ignored and doesn't produce any CMake commands.
    name: lambda *kargs, **kwargs: None
    for name in [
        # keep sorted go/buildifier#keep-sorted
        "build_test",
        "cc_proto_library",
        "cc_stubby_library",
        "java_proto_library",
        "java_stubby_library",
        "load",
        "package",
        "proto_library",
        "sh_binary",
        "sh_test",
    ]
}


def process_file(prefix: str, file: str):
    """Processes a BUILD file and generates a CMakeBazel.txt file."""
    assert os.path.isfile(file)
    assert os.path.basename(file) == "BUILD.bazel"
    with open(file, "r") as f:
        lines = f.read()
    global CURRENT_CMAKE_PIECES
    CURRENT_CMAKE_PIECES = []
    CURRENT_CMAKE_PIECES.append(
        f"# This file is auto generated by bazel2cmake.py from {file}\n"
        "# Don't edit manually, your changes will be lost.\n"
        "# You can update this file by running:\n"
        f"#   python3 tools/build/bazel2cmake.py {file}\n"
    )
    global CURRENT_FOLDER
    CURRENT_FOLDER = os.path.dirname(file)
    global CURRENT_TARGET_PREFIX
    CURRENT_TARGET_PREFIX = prefix
    exec(lines, EXEC_GLOBALS)  # pylint: disable=exec-used
    output_file = os.path.join(os.path.dirname(file), "CMakeBazel.txt")
    with open(output_file, "w") as f:
        f.write("\n\n".join(CURRENT_CMAKE_PIECES))


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        print("bazel2cmake takes no arguments")
        return
    # TODO: Add more bazel files to autogenerate.
    process_file("bzl_cc_example_", "examples/cpp/BUILD.bazel")


if __name__ == "__main__":
    app.run(main)
