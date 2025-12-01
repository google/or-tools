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

"""Driver to test binaries with a script."""

from collections.abc import Sequence
import os
import re
import shlex
import subprocess
import sys

from tools.testing import bintest_matchers
from tools.testing import bintest_run_utils


class ScriptError(Exception):
    pass


class Re:

    STRING = bintest_matchers.Re.STRING


def line_error(line_number: int, line: str, msg: str) -> ScriptError:
    """Creates a ScriptError for a line with a message."""
    return ScriptError(f"At l.{line_number}, {line!r}\n{msg}")


def main(argv: Sequence[str]):
    try:
        if len(argv) != 2:
            raise ScriptError("Missing script path")

        script_path = argv[1]

        if not os.path.isfile(script_path):
            raise FileNotFoundError(f"File not found: {script_path}")

        with open(script_path, "r") as f:
            lines = tuple(f)

        last_cmd_line = None
        last_run_output = None
        for line_number, line in enumerate(lines, start=1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("RUN:"):
                command = line.removeprefix("RUN:").strip()
                if not command:
                    raise line_error(line_number, line, "missing RUN command")
                try:
                    args = shlex.split(command)
                except Exception as e:
                    raise line_error(line_number, line, str(e)) from None
                last_cmd_line = line
                try:
                    last_run_output = bintest_run_utils.run(*args)
                except subprocess.CalledProcessError as e:
                    raise line_error(
                        line_number,
                        line,
                        f"Binary failed to run with return code: {e.returncode}.\n"
                        f"The standard error was: {e.stderr!r}.\n"
                        f"The standard output was: {e.stdout!r}.\n",
                    ) from None
                continue
            if line.startswith("CHECK:"):
                matchers = line.removeprefix("CHECK:").strip()
                if not matchers:
                    raise line_error(line_number, line, "missing CHECK matchers")
                if last_run_output is None:
                    raise line_error(line_number, line, "CHECK without previous RUN")
                if match := re.findall(Re.STRING, matchers):
                    arg_list = [string[1:-1] for string in match]
                    try:
                        bintest_matchers.extract(
                            last_run_output, *arg_list, check_matcher_specs=True
                        )
                    except bintest_matchers.MatchError as e:
                        message = (
                            f"command : {last_cmd_line!r}\n"  #
                            f"output  : {last_run_output!r}\n"  #
                            f"error   : {e}"
                        )
                        raise line_error(line_number, line, message) from None
                    continue
                raise line_error(line_number, line, "CHECK requires quoted strings")
            raise line_error(line_number, line, "unknown directive")
        if not last_cmd_line:
            raise ScriptError("No RUN directive found.")
    except FileNotFoundError as e:
        sys.exit(str(e))
    except ScriptError as e:
        sys.exit(str(e))
