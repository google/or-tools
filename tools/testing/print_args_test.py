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

"""Tests for testing/binary_test.py."""

from tools.testing import binary_test
from tools.testing import bintest_run_utils


# [START program]
class BinaryTestingPyTest(binary_test.BinaryTestCase):
    """Tests for testing/binary_test.py."""

    def test_run_no_args(self):
        # Running the binary with no argument prints "Hello world!"
        out = self.assert_binary_succeeds("$(print_args)")
        self.assert_extract(out, "Hello world!")

    def test_run_get_single_float(self):
        # Running the binary with arguments also prints the arguments verbatim.
        out = self.assert_binary_succeeds("$(print_args)", "--value=0.99999999")
        value = self.assert_has_line_with_prefixed_number("--value=", out)
        self.assertAlmostEqual(value, 1.0)

    def test_check_text_and_extract_float(self):
        # Running the binary with arguments also prints the arguments verbatim.
        out = self.assert_binary_succeeds("$(print_args)", "--value=0.99999999")
        (value,) = self.assert_extract(out, "Hello world!", "--value=@num()")
        self.assertAlmostEqual(value, 1.0)

    def test_check_extract_several_floats(self):
        # Running the binary with arguments also prints the arguments verbatim.
        out = self.assert_binary_succeeds("$(print_args)", "a=1", "b=54.7", "c=12")
        (a, b, c) = self.assert_extract(out, "a=@num()", "b=@num()", "c=@num()")
        self.assertGreater(a, 0)
        self.assertAlmostEqual(b, 55, delta=1)
        self.assertBetween(c, 10, 15)

    def test_run_named_args(self):
        # Passing a named variable should be expanded
        out = self.assert_binary_succeeds("$(print_args)", "--input=$(print_args)")
        self.assert_extract(out, "print_args")

    def test_refer_to_side_data(self):
        # Passing a named variable should be expanded
        out = self.assert_binary_succeeds("$(print_args)", "--input=$(data_file)")
        self.assert_extract(out, "print_args_data.txt")

    def test_read_side_data(self):
        path = bintest_run_utils.get_path("data_file")
        with open(path, "r") as f:
            self.assertEqual(f.read(), "This file contains side data.")

    def test_capture_stderr(self):
        # Passing "2>&1" allows to also capture the error stream.
        out = self.assert_binary_succeeds(
            "$(print_args)", "--stderr", "error message", "2>&1"
        )
        self.assert_extract(out, "error message")

    def test_failure(self):
        self.assert_binary_fails("$(print_args)", "--fail")


# [END program]


if __name__ == "__main__":
    binary_test.main()
