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

import sys
from typing import Optional
from absl.testing import absltest
from tools.testing import bintest_script_runner


class BintestScriptRunnerTest(absltest.TestCase):

    def execute_with_path(self, file_path: Optional[str]):
        args = ["runner.py"]
        if file_path is not None:
            args.append(file_path)
        bintest_script_runner.main(args)

    def execute_with_script_content(self, content: str):
        name = f"{hash(content)}.bintest"
        script = self.create_tempfile(file_path=name, content=content)
        self.execute_with_path(script.full_path)

    def test_missing_script_path(self):
        with self.assertRaisesRegex(SystemExit, "Missing script path"):
            self.execute_with_path(None)

    def test_non_existing_script_path(self):
        with self.assertRaisesRegex(SystemExit, "/non/existing/script.sh"):
            self.execute_with_path("/non/existing/script.sh")

    def test_empty_script(self):
        with self.assertRaisesRegex(SystemExit, "No RUN directive found"):
            self.execute_with_script_content("")

    def test_invalid_directive(self):
        with self.assertRaisesRegex(SystemExit, "unknown directive"):
            self.execute_with_script_content("DEBUG: something")

    def test_check_without_run(self):
        with self.assertRaisesRegex(SystemExit, "CHECK without previous RUN"):
            self.execute_with_script_content("CHECK: 'something'")

    def test_run_no_args(self):
        with self.assertRaisesRegex(SystemExit, "missing RUN command"):
            self.execute_with_script_content("RUN:")

    def test_no_closing_args(self):
        with self.assertRaisesRegex(SystemExit, "No closing quotation"):
            self.execute_with_script_content("RUN: 'foo")

    def test_run_ok(self):
        self.execute_with_script_content("RUN: $(ECHO)")

    def test_run_inexistent_binary(self):
        with self.assertRaisesRegex(SystemExit, "No such file or directory"):
            self.execute_with_script_content("RUN: /path/to/inexistent_binary")

    def test_run_fails(self):
        with self.assertRaisesRegex(SystemExit, "failed to run"):
            self.execute_with_script_content("RUN: $(FAIL)")

    def test_run_and_check(self):
        self.execute_with_script_content(
            """
RUN: $(ECHO) foo bar baz boo
CHECK: "foo" "baz"
CHECK: "bar" "boo"
"""
        )

    def test_run_and_fail_match(self):
        with self.assertRaisesRegex(SystemExit, "No match for"):
            self.execute_with_script_content(
                """
RUN: $(ECHO) foo bar
CHECK: "baz"
"""
            )

    def test_run_and_unquoted_check(self):
        with self.assertRaisesRegex(SystemExit, "CHECK requires quoted strings"):
            self.execute_with_script_content(
                """
RUN: $(ECHO) 'foo' 'bar'
CHECK: foo
"""
            )

    def test_run_and_empty_check(self):
        with self.assertRaisesRegex(SystemExit, "missing CHECK matchers"):
            self.execute_with_script_content(
                """
RUN: $(ECHO) 'foo' 'bar'
CHECK:
"""
            )


if __name__ == "__main__":
    absltest.main()
