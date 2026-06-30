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
"""

import dataclasses
import os
import re
import sys
from collections.abc import Sequence

ROOT_FOLDER = os.getcwd()  # The project root.

# ##############################################################################
# Extraction of bazel call from a Bazel file.


@dataclasses.dataclass(eq=True, frozen=True)
class BazelCall:
    """A bazel call.

    Attributes:
      name: The name of the function called.
      kwargs: The arguments of the call.
    """

    name: str
    kwargs: dict[str, str]


def extract_bazel_calls(
    content: str,
    extract_functions: Sequence[str],
    discard_functions: Sequence[str],
) -> Sequence[BazelCall]:
    """Extracts bazel calls from a BUILD.bazel file.

    Args:
      content: The content of the BUILD.bazel file.
      extract_functions: A set of function names to record.
      discard_functions: A set of function names to discard.

    Returns:
      A sequence of BazelCall objects corresponding to the extracted functions.
    """
    calls = []

    def record(action: str):
        return lambda *kargs, **kwargs: calls.append(BazelCall(action, kwargs))

    def discard():
        return lambda *kargs, **kwargs: None

    # pylint: disable=exec-used
    exec(
        content,
        {"__builtins__": {}}
        | {foo: record(foo) for foo in extract_functions}
        | {foo: discard() for foo in discard_functions}
        | {"requirement": lambda value: f"requirement({value})"},
        {},
    )
    return calls


# ##############################################################################
# Conversion of bazel targets to CMake targets.


@dataclasses.dataclass(eq=True, order=True, frozen=True)
class BazelTarget:
    """A bazel target.

    Attributes:
      repo: The repository of the target, e.g. "@ortools".
      pkg: The package of the target, e.g. "//ortools/base".
      id: The name of the target, e.g. "statusor".
    """

    repo: str
    pkg: str  # The package of the target.
    id: str  # The name of the target.

    def __str__(self):
        return f"{self.repo}{self.pkg}{":"+self.id if self.id else ''}"

    def path(self) -> str:
        assert self.repo == "@ortools"
        return os.path.join(self.pkg[2:], self.id)

    def is_file(self) -> bool:
        return os.path.isfile(os.path.join(ROOT_FOLDER, self.path()))


_PKG = r"\/\/[_a-zA-Z0-9-\/]*"
_REPO = r"@[_a-zA-Z0-9-]+"
_FILE_CC = r"[_a-zA-Z0-9-\.]"
_REL_FILE = f"{_FILE_CC}+(?:\\/{_FILE_CC}+)*"
_BZL_TARGET_RE = re.compile(f"({_REPO})?({_PKG})?(?::({_REL_FILE}))?")


def parse_bazel_label(label: str, current_pkg: str = "") -> BazelTarget:
    """Parses a bazel label.

    Args:
      label: The bazel label to parse.
      current_pkg: The default package to use if the label is relative.

    Returns:
      A BazelTarget object.
    """
    if (
        not label.startswith("//")
        and not label.startswith("@")
        and not label.startswith(":")
    ):
        label = f":{label}"
    if match := _BZL_TARGET_RE.fullmatch(label):
        repo = match[1] or "@ortools"
        pkg = match[2] or (current_pkg if repo == "@ortools" else "")
        return BazelTarget(repo, pkg, match[3] or "")
    else:
        raise ValueError(f"Invalid label: {label}")


@dataclasses.dataclass(eq=True, order=True, frozen=True)
class CMakeTarget:
    """A CMake target.

    Attributes:
      namespace: The repository of the target.
      id: The name of the target.
    """

    namespace: str
    id: str

    def __str__(self):
        return f"{self.namespace}::{self.id}" if self.namespace else self.id


def parse_cmake_label(label: str) -> CMakeTarget:
    """Parses a CMake label.

    Args:
      label: The CMake label to parse.

    Returns:
      A CMakeTarget object.
    """
    idx = label.find("::")
    if idx == -1:
        return CMakeTarget("", label)
    return CMakeTarget(label[:idx], label[idx + 2 :])


# ##############################################################################
# Conversion configuration

# If True add third party libraries (abseil, re2, ...) to LINK_LIBRARIES
ENABLE_3RD_PARTY_LIBRARIES = False

# If True add ortools_<component> libraries to LINK_LIBRARIES
ENABLE_ORTOOLS_LIBRARIES = False

# A map of ortools packages to target prefixes.
ORTOOLS_PKG_TO_CMAKE_PREFIX = {
    "//examples/cpp": "bzl_cc_example_",
    "//examples/python": "bzl_py_example_",
}

# A map of ortools packages to cmake libraries bundles.
ORTOOLS_PKG_TO_CMAKE = {
    "//ortools/algorithms": "ortools_algorithms",
    "//ortools/base": "ortools_base",
    "//ortools/constraint_solver": "ortools_constraint_solver",
    "//ortools/glop": "ortools_glop",
    "//ortools/graph_base": "ortools_graph_base",
    "//ortools/graph": "ortools_graph",
    "//ortools/linear_solver": "ortools_linear_solver",
    "//ortools/lp_data": "ortools_lp_data",
    "//ortools/packing": "ortools_packing",
    "//ortools/pdlp": "ortools_pdlp",
    "//ortools/port": "ortools_port",
    "//ortools/routing": "ortools_routing",
    "//ortools/routing/parsers": "ortools_routing_parsers",
    "//ortools/sat": "ortools_sat",
    "//ortools/scheduling": "ortools_scheduling",
    "//ortools/util": "ortools_util",
}

# A map of bazel targets to their CMake counterpart.
LABEL_MAPPING = {
    "@abseil-cpp//absl/algorithm:container": "absl::algorithm_container",
    "@abseil-cpp//absl/base:log_severity": "absl::log_severity",
    "@abseil-cpp//absl/base": "absl::base",
    "@abseil-cpp//absl/cleanup": "absl::cleanup",
    "@abseil-cpp//absl/container:btree": "absl::btree",
    "@abseil-cpp//absl/container:common": "absl::container_common",
    "@abseil-cpp//absl/container:fixed_array": "absl::fixed_array",
    "@abseil-cpp//absl/container:flat_hash_map": "absl::flat_hash_map",
    "@abseil-cpp//absl/container:flat_hash_set": "absl::flat_hash_set",
    "@abseil-cpp//absl/container:inlined_vector": "absl::inlined_vector",
    "@abseil-cpp//absl/container:node_hash_map": "absl::node_hash_map",
    "@abseil-cpp//absl/debugging:leak_check": "absl::leak_check",
    "@abseil-cpp//absl/flags:flag": "absl::flags",
    "@abseil-cpp//absl/flags:marshalling": "absl::flags_marshalling",
    "@abseil-cpp//absl/flags:parse": "absl::flags_parse",
    "@abseil-cpp//absl/flags:usage": "absl::flags_usage",
    "@abseil-cpp//absl/functional:any_invocable": "absl::any_invocable",
    "@abseil-cpp//absl/functional:bind_front": "absl::bind_front",
    "@abseil-cpp//absl/functional:function_ref": "absl::function_ref",
    "@abseil-cpp//absl/hash:hash_testing": "absl::hash_testing",
    "@abseil-cpp//absl/hash": "absl::hash",
    "@abseil-cpp//absl/log:check": "absl::check",
    "@abseil-cpp//absl/log:die_if_null": "absl::die_if_null",
    "@abseil-cpp//absl/log:flags": "absl::log_flags",
    "@abseil-cpp//absl/log:globals": "absl::log_globals",
    "@abseil-cpp//absl/log:initialize": "absl::log_initialize",
    "@abseil-cpp//absl/log:log_streamer": "absl::log_streamer",
    "@abseil-cpp//absl/log:scoped_mock_log": "absl::scoped_mock_log",
    "@abseil-cpp//absl/log:vlog_is_on": "absl::vlog_is_on",
    "@abseil-cpp//absl/log": "absl::log",
    "@abseil-cpp//absl/memory": "absl::memory",
    "@abseil-cpp//absl/meta:type_traits": "absl::meta",
    "@abseil-cpp//absl/numeric:bits": "absl::bits",
    "@abseil-cpp//absl/numeric:int128": "absl::int128",
    "@abseil-cpp//absl/random:bit_gen_ref": "absl::random_bit_gen_ref",
    "@abseil-cpp//absl/random:distributions": "absl::random_distributions",
    "@abseil-cpp//absl/random:seed_sequences": "absl::random_seed_sequences",
    "@abseil-cpp//absl/random": "absl::random_random",
    "@abseil-cpp//absl/status:status_matchers": "absl::status_matchers",
    "@abseil-cpp//absl/status:statusor": "absl::statusor",
    "@abseil-cpp//absl/status": "absl::status",
    "@abseil-cpp//absl/strings:str_format": "absl::str_format",
    "@abseil-cpp//absl/strings:string_view": "absl::string_view",
    "@abseil-cpp//absl/strings": "absl::strings",
    "@abseil-cpp//absl/synchronization": "absl::synchronization",
    "@abseil-cpp//absl/time": "absl::time",
    "@abseil-cpp//absl/types:optional": "absl::optional",
    "@abseil-cpp//absl/types:span": "absl::span",
    "@abseil-cpp//absl/utility": "absl::utility",
    "@protobuf": "protobuf::libprotobuf",
    "@protobuf//:wrappers_cc_proto": "protobuf::libprotobuf",
    "@re2": "${RE2_DEPS}",
}

BAZEL2CMAKE = {
    parse_bazel_label(key): parse_cmake_label(value)
    for key, value in LABEL_MAPPING.items()
}

NULL_TARGET = CMakeTarget("", "")


def bzl2cmake(target: BazelTarget) -> CMakeTarget:
    """Converts a bazel target to a CMake target.

    This function is responsible for merging individual bazel targets into coarser
    CMake libraries. e,g,. @ortools//ortools/base:... -> ortools_base

    Args:
      target: The bazel target to convert.

    Returns:
      A CMakeTarget object.
    """
    if target.repo == "@ortools":
        if prefix := ORTOOLS_PKG_TO_CMAKE_PREFIX.get(target.pkg):
            return CMakeTarget("", f"{prefix}{target.id}")
        if target.pkg in ORTOOLS_PKG_TO_CMAKE:
            return (
                CMakeTarget("", ORTOOLS_PKG_TO_CMAKE[target.pkg])
                if ENABLE_ORTOOLS_LIBRARIES
                else NULL_TARGET
            )
        raise ValueError(f"Unknown target: {target}")
    assert target in BAZEL2CMAKE, f"Unknown target: {target}"
    return BAZEL2CMAKE[target] if ENABLE_3RD_PARTY_LIBRARIES else NULL_TARGET


@dataclasses.dataclass
class BazelContext:
    """The context of a bazel package.

    Attributes:
      folder: The folder of the package.
      pkg: The package name.
    """

    folder: str
    pkg: str

    def __init__(self, folder: str):
        self.folder = folder
        self.pkg = f"//{self.folder}"

    def _bzl_target(self, label: str) -> BazelTarget:
        return parse_bazel_label(label, self.pkg)

    def _cmake_file(self, label: str) -> str:
        target = self._bzl_target(label)
        if target.is_file():
            path = target.path()
            current_folder = self.folder + "/"
            if not path.startswith(current_folder):
                return "${CMAKE_SOURCE_DIR}/" + path
            path = path.removeprefix(current_folder)
            return "${CMAKE_CURRENT_SOURCE_DIR}/" + path
        else:
            return f"$<TARGET_FILE:{bzl2cmake(target)}>"

    def _targets(self, labels: list[str]) -> list[CMakeTarget]:
        out = []
        for label in labels:
            if target := bzl2cmake(self._bzl_target(label)):
                out.append(target)
        return out

    def cmake_deps(self, labels: list[str]) -> list[str]:
        return sorted(
            [
                str(target)
                for target in set(self._targets(labels))
                if target is not NULL_TARGET
            ]
        )

    def cmake_files(self, labels: list[str]) -> list[str]:
        return [self._cmake_file(label) for label in labels]

    def cmake_name(self, name: str) -> list[str]:
        return [str(cmake_target) for cmake_target in self._targets([f":{name}"])]

    def cmake_env(self, named_data: dict[str, str]) -> list[str]:
        return [
            f"BINTEST_{key}={self._cmake_file(target)}"
            for key, target in named_data.items()
        ]


@dataclasses.dataclass
class CMakeFmt:
    """A CMake file formatter.

    Attributes:
      pieces: The pieces of the CMake file.
    """

    pieces: list[str]

    def __str__(self):
        return "\n".join(self.pieces)

    def add_line(self, piece: str, indent: int = 0) -> None:
        self.pieces.append("  " * indent + piece)


@dataclasses.dataclass
class CMakeArg:
    """A CMake argument.

    Attributes:
      name: The name of the argument.
      values: The values of the argument.
      remove_if_empty: Whether to remove the argument if it has no values.
    """

    name: str
    values: list[str]
    remove_if_empty: bool = True

    def format(self, fmt: CMakeFmt) -> None:
        if not self.name:
            return
        if not self.values:
            if not self.remove_if_empty:
                fmt.add_line(self.name, indent=1)
        elif len(self.values) == 1:
            fmt.add_line(f"{self.name} {self.values[0]}", indent=1)
        else:
            fmt.add_line(f"{self.name}", indent=1)
            for value in self.values:
                fmt.add_line(value, indent=2)


@dataclasses.dataclass
class CMakeCall:
    """A CMake call.

    Attributes:
      name: The name of the function called.
      args: The arguments of the call.
    """

    name: str
    args: list[CMakeArg]

    def __init__(self, name: str, *args: CMakeArg):
        self.name = name
        self.args = list(args)

    def format(self, fmt: CMakeFmt) -> None:
        """Formats the CMake call.

        Args:
          fmt: The CMakeFmt object to use for formatting.
        """
        fmt.add_line(f"{self.name}(")
        for arg in self.args:
            arg.format(fmt)
        fmt.add_line(")")


# ##############################################################################
# Conversion of bazel targets to CMake targets.


def map_bintest(ctx: BazelContext, name: str, **kwargs) -> CMakeCall:
    srcs = kwargs.get("srcs", [])
    named_data = kwargs.get("named_data", {})
    args = [
        CMakeArg("NAME", ctx.cmake_name(name)),
        CMakeArg("SCRIPT", ctx.cmake_files(srcs)),
        CMakeArg("ENVIRONMENT", ctx.cmake_env(named_data)),
    ]
    if "noci" in kwargs.get("tags", []):
        args.append(CMakeArg(name="DISABLED", values=[], remove_if_empty=False))
    return CMakeCall("ortools_cxx_bintest", *args)


def map_cc_binary(ctx: BazelContext, name: str, **kwargs) -> CMakeCall:
    srcs = kwargs.get("srcs", [])
    hdrs = kwargs.get("hdrs", [])
    deps = kwargs.get("deps", [])
    return CMakeCall(
        "ortools_cxx_binary",
        CMakeArg("NAME", ctx.cmake_name(name)),
        CMakeArg("SOURCES", sorted(srcs + hdrs)),
        CMakeArg("LINK_LIBRARIES", ctx.cmake_deps(deps)),
    )


def map_cc_library(ctx: BazelContext, name: str, **kwargs) -> CMakeCall:
    srcs = kwargs.get("srcs", [])
    hdrs = kwargs.get("hdrs", [])
    deps = kwargs.get("deps", [])
    return CMakeCall(
        "ortools_cxx_library",
        CMakeArg("NAME", ctx.cmake_name(name)),
        CMakeArg("SOURCES", sorted(srcs + hdrs)),
        CMakeArg("LINK_LIBRARIES", ctx.cmake_deps(deps)),
        CMakeArg("TYPE", ["INTERFACE" if not srcs else "SHARED"]),
    )


def map_py_binary(ctx: BazelContext, name: str, **kwargs) -> CMakeCall:
    srcs = kwargs.get("srcs", [])
    return CMakeCall(
        "add_python_binary",
        CMakeArg("NAME", ctx.cmake_name(name)),
        CMakeArg("FILE", ctx.cmake_files(srcs)),
    )


CALL_MAPPERS = {
    "bintest": map_bintest,
    "build_test": None,
    "cc_binary": map_cc_binary,
    "cc_library": map_cc_library,
    "cc_proto_library": None,
    "cc_stubby_library": None,
    "cc_test": None,
    "filegroup": None,
    "java_proto_library": None,
    "java_stubby_library": None,
    "load": None,
    "package": None,
    "proto_library": None,
    "py_binary": map_py_binary,
    "select": None,
    "sh_binary": None,
    "sh_test": None,
    "SAFE_FP_CODE": None,
}
EXTRACT_FUNCTIONS = [key for key, value in CALL_MAPPERS.items() if value]
DISCARD_FUNCTIONS = [key for key, value in CALL_MAPPERS.items() if not value]


FILES_TO_PROCESS = [
    "examples/cpp/BUILD.bazel",
    "examples/python/BUILD.bazel",
]


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        print("bazel2cmake takes no arguments")
        return

    for file in FILES_TO_PROCESS:
        with open(file, "r") as f:
            bazel_content = f.read()
        folder = os.path.dirname(file)
        ctx = BazelContext(folder)
        bazel_calls = extract_bazel_calls(
            bazel_content, EXTRACT_FUNCTIONS, DISCARD_FUNCTIONS
        )
        cmake_calls = [
            CALL_MAPPERS[call.name](ctx, **call.kwargs) for call in bazel_calls
        ]
        fmt = CMakeFmt(
            [
                f"# This file is auto generated by bazel2cmake.py from {file}",
                "# Don't edit manually, your changes will be lost.",
                "# You can update this file by running:",
                f"#   python3 tools/build/bazel2cmake.py {file}",
                "",
            ]
        )
        for call in cmake_calls:
            call.format(fmt)
            fmt.add_line("")
        output_file = os.path.join(folder, "CMakeBazel.txt")
        with open(output_file, "w") as f:
            print(f"Writing {output_file}")
            f.write(str(fmt))


if __name__ == "__main__":
    main(sys.argv)
