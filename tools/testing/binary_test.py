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

"""Base class for binary tests."""

from collections.abc import Sequence
import re
import subprocess

from absl.testing import absltest
from tools.testing import bintest_matchers
from tools.testing import bintest_run_utils


class BinaryTestCase(absltest.TestCase):
    """Base class for testing binaries.

    Provides utility methods for running binaries, getting paths to data
    dependencies (defined in the `named_data` attribute of the build rule),
    and extracting information from logs.
    """

    def assert_binary_succeeds(self, binary: str, *args: str) -> str:
        """Runs a command, expanding $(label) expressions, and returns the output.

        `binary` and `args` can contain expressions of the form `$(label)`. These
        expressions are replaced by the absolute path of the data dependency
        associated with `label` in the `named_data` attribute of the build rule.

        See `bintest_run_utils.run` for more details.

        Args:
          binary: The path to the binary to execute.
          *args: Arguments to pass to the command.

        Returns:
          The standard output of the command execution.

        Raises:
          ValueError: If any label expansion fails.
        """
        try:
            return bintest_run_utils.run(binary, *args)
        except subprocess.CalledProcessError as e:
            self.fail(
                f"Expected {binary!r} to succeed but it failed with a return code"
                f" {e.returncode}.\n"
                f"The expanded command line was: {e.cmd!r}.\n"
                f"The standard error was: {e.stderr!r}.\n"
                f"The standard output was: {e.stdout!r}.\n"
            )

    def assert_binary_fails(
        self, binary: str, *args: str
    ) -> subprocess.CalledProcessError:
        """Runs a command, expanding $(label) expressions, and asserts that it fails.

        `binary` and `args` can contain expressions of the form `$(label)`. These
        expressions are replaced by the absolute path of the data dependency
        associated with `label` in the `named_data` attribute of the build rule.

        See `bintest_run_utils.run` for more details.

        Args:
          binary: The path to the binary to execute.
          *args: Arguments to pass to the command.

        Returns:
          The subprocess.CalledProcessError raised by the command execution.

        Raises:
          ValueError: If any label expansion fails.
        """
        with self.assertRaises(subprocess.CalledProcessError) as e:
            bintest_run_utils.run(binary, *args)
        return e.exception

    def assert_extract(self, log: str, *patterns: str) -> Sequence[float]:
        """Extracts floating point numbers from a log string based on patterns.

        Patterns are applied sequentially. Each pattern can contain "@num()"
        placeholders, which are replaced by a regex to capture floating-point
        numbers.

        See `bintest_matchers.extract` for more details.

        Args:
          log: The log string to search within.
          *patterns: Pattern strings to search for sequentially.

        Returns:
          A tuple of floating point numbers extracted from the log, in the order of
          their `@num()` matchers in `patterns`.

        Raises:
          AssertionError: If any pattern is not found or if number extraction fails.
        """
        try:
            return bintest_matchers.extract(log, *patterns, check_matcher_specs=False)
        except bintest_matchers.MatchError as e:
            self.fail(e)

    def assert_has_line_with_prefixed_number(self, prefix: str, log: str) -> float:
        """Extracts a floating point number from a log line starting with a prefix.

        Args:
          prefix: The prefix of the line to search for.
          log: The log string to search within.

        Returns:
          The floating point number extracted from the line.
        """
        regex = re.escape(prefix) + " *" + bintest_matchers.Re.NUMBER
        if match := re.search(regex, log):
            return float(match[1])
        self.fail(f"No line starting with {prefix!r} found in {log!r}")


def main() -> None:
    absltest.main()
