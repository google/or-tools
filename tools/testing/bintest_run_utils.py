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

"""Utilities for binary testing."""

import os
import re
import subprocess

_SUBSTITUTION_REGEX = re.compile(r"\$\(([^)]+)?\)")
_ENV_PREFIX = "BINTEST_"


def get_path(label: str) -> str:
    """Returns the path to a file from an environment variable.

    Args:
      label: The label to access the path to the file.

    Returns:
      The absolute path to the file.

    Raises:
      ValueError: If the environment variable corresponding to the label is not
        set or if the file does not exist at the specified path.
    """
    file_path = os.getenv(f"{_ENV_PREFIX}{label}")
    if not file_path:
        available_labels = []
        for value in os.environ:
            if value.startswith(_ENV_PREFIX):
                available_labels.append(value.removeprefix(_ENV_PREFIX))
        raise ValueError(
            f"Environment variable for {label} not set. Available labels :"
            f" {available_labels}\nYou may need to check the build rule"
            ' "named_data" attribute.'
        )
    if not os.path.isfile(file_path):
        raise ValueError(f"File {file_path} not found.")
    return file_path


def _expand_path(arg: str) -> str:
    return _SUBSTITUTION_REGEX.sub(lambda match: get_path(match[1]), arg)


def run(binary: str, *args: str) -> str:
    """Runs the command denoted by args.

    Args:
      binary: The path to the binary to execute.
      *args: Arguments of the command line. The arguments may contain `$(label)`
        expressions which will be replaced by the value of the environment
        variable `BINTEST_label`. These values are set by the build system based
        on the `named_data` attribute. If the argument `2>&1` is present, stderr
        will be redirected to stdout.

    Returns:
      The stdout output of the binary as a string. If `2>&1` is used, stderr will
      be appended to stdout.

    Raises:
      ValueError: If any of the path expansion fails.
      subprocess.CalledProcessError: If the binary returns a non-zero exit code.
    """

    also_stderr = False
    args_out = [_expand_path(binary)]
    for arg in args:
        if arg == "2>&1":
            also_stderr = True
        else:
            args_out.append(_expand_path(arg))
    result = subprocess.run(
        args_out,
        encoding="utf-8",
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT if also_stderr else subprocess.PIPE,
        check=True,
    )
    return result.stdout
