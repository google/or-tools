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

"""Unit tests for bintest_run_utils."""

import os
import subprocess
from unittest import mock

from absl.testing import absltest
from tools.testing import bintest_run_utils


class BintestRunUtilsTest(absltest.TestCase):

    def test_get_path_env_var_not_set(self):
        with mock.patch.dict(os.environ, {}, clear=True):
            with self.assertRaisesRegex(
                ValueError, "Environment variable for MY_LABEL not set"
            ):
                bintest_run_utils.get_path("MY_LABEL")

    def test_get_path_env_var_set_and_file_exists(self):
        file_path = self.create_tempfile().full_path
        with mock.patch.dict(os.environ, {"BINTEST_MY_LABEL": file_path}):
            self.assertEqual(bintest_run_utils.get_path("MY_LABEL"), file_path)

    def test_get_path_env_var_set_and_file_does_not_exists(self):
        with mock.patch.dict(os.environ, {"BINTEST_MY_LABEL": "/path/to/file"}):
            with self.assertRaisesRegex(Exception, "not found"):
                bintest_run_utils.get_path("MY_LABEL")

    @mock.patch.object(subprocess, "run")
    def test_run_simple(self, mock_run):
        binary_path = self.create_tempfile().full_path
        with mock.patch.dict(os.environ, {"BINTEST_binary": binary_path}):
            mock_run.return_value = subprocess.CompletedProcess(
                args=[binary_path, "arg1", "arg2"],
                returncode=0,
                stdout="output",
                stderr="",
            )
            self.assertEqual(
                bintest_run_utils.run("$(binary)", "arg1", "arg2"), "output"
            )
            mock_run.assert_called_once_with(
                [binary_path, "arg1", "arg2"],
                encoding="utf-8",
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=True,
            )

    @mock.patch.object(subprocess, "run")
    def test_run_with_arg_expansion(self, mock_run):
        binary_path = self.create_tempfile().full_path
        data_path = self.create_tempfile().full_path
        with mock.patch.dict(
            os.environ,
            {"BINTEST_binary": binary_path, "BINTEST_data": data_path},
        ):
            mock_run.return_value = subprocess.CompletedProcess(
                args=[binary_path, data_path],
                returncode=0,
                stdout="output",
                stderr="",
            )
            self.assertEqual(bintest_run_utils.run("$(binary)", "$(data)"), "output")
            mock_run.assert_called_once_with(
                [binary_path, data_path],
                encoding="utf-8",
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=True,
            )

    @mock.patch.object(subprocess, "run")
    def test_run_with_stderr_redirect(self, mock_run):
        binary_path = self.create_tempfile().full_path
        with mock.patch.dict(os.environ, {"BINTEST_binary": binary_path}):
            mock_run.return_value = subprocess.CompletedProcess(
                args=[binary_path],
                returncode=0,
                stdout="output",
                stderr="",
            )
            self.assertEqual(bintest_run_utils.run("$(binary)", "2>&1"), "output")
            mock_run.assert_called_once_with(
                [binary_path],
                encoding="utf-8",
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=True,
            )

    @mock.patch.object(subprocess, "run")
    def test_run_with_failure(self, mock_run):
        mock_run.side_effect = subprocess.CalledProcessError(
            1, ["binary"], "output", "error"
        )
        with self.assertRaises(subprocess.CalledProcessError):
            bintest_run_utils.run("binary")


if __name__ == "__main__":
    absltest.main()
