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

import io
import os

from absl import logging

from absl.testing import absltest
from ortools.math_opt.python import message_callback


class PrinterMessageCallbackTest(absltest.TestCase):

  def test_no_prefix(self):
    class FlushCountingStringIO(io.StringIO):

      def __init__(self):
        super().__init__()
        self.num_flushes: int = 0

      def flush(self):
        super().flush()
        self.num_flushes += 1

    buf = FlushCountingStringIO()
    cb = message_callback.printer_message_callback(file=buf)
    cb(["line 1", "line 2"])
    cb(["line 3"])

    self.assertMultiLineEqual(buf.getvalue(), "line 1\nline 2\nline 3\n")
    self.assertEqual(buf.num_flushes, 2)

  def test_with_prefix(self):
    buf = io.StringIO()
    cb = message_callback.printer_message_callback(file=buf, prefix="test> ")
    cb(["line 1", "line 2"])
    cb(["line 3"])

    self.assertMultiLineEqual(
        buf.getvalue(), "test> line 1\ntest> line 2\ntest> line 3\n"
    )


class LogMessagesTest(absltest.TestCase):

  def test_defaults(self):
    with self.assertLogs(logger="absl", level="INFO") as logs:
      message_callback.log_messages(["line 1", "line 2"])
    self.assertListEqual(logs.output, ["INFO:absl:line 1", "INFO:absl:line 2"])

  def test_prefix(self):
    with self.assertLogs(logger="absl") as logs:
      message_callback.log_messages(["line 1", "line 2"], prefix="solver: ")
    self.assertListEqual(
        logs.output, ["INFO:absl:solver: line 1", "INFO:absl:solver: line 2"]
    )

  def test_warning(self):
    with self.assertLogs(logger="absl") as logs:
      message_callback.log_messages(["line 1", "line 2"], level=logging.WARNING)
    self.assertListEqual(
        logs.output, ["WARNING:absl:line 1", "WARNING:absl:line 2"]
    )

  def test_records_path(self):
    with self.assertLogs(logger="absl") as logs:
      message_callback.log_messages(["line 1", "line 2"])
    self.assertSetEqual(
        set(os.path.basename(r.pathname) for r in logs.records),
        set(("message_callback_test.py",)),
    )


class VLogMessagesTest(absltest.TestCase):
  """Tests of vlog_messages().

  In the tests we abuse the logging level 0 since there is not API in the
  `logging` module to change the verbosity.
  """

  def test_defaults(self):
    with self.assertLogs(logger="absl") as logs:
      message_callback.vlog_messages(["line 1", "line 2"], 0)
    self.assertListEqual(logs.output, ["INFO:absl:line 1", "INFO:absl:line 2"])

  def test_prefix(self):
    with self.assertLogs(logger="absl") as logs:
      message_callback.vlog_messages(["line 1", "line 2"], 0, prefix="solver: ")
    self.assertListEqual(
        logs.output, ["INFO:absl:solver: line 1", "INFO:absl:solver: line 2"]
    )

  def test_records_path(self):
    with self.assertLogs(logger="absl") as logs:
      message_callback.vlog_messages(["line 1", "line 2"], 0)
    self.assertSetEqual(
        set(os.path.basename(r.pathname) for r in logs.records),
        set(("message_callback_test.py",)),
    )


class ListMessageCallbackTest(absltest.TestCase):

  def test_empty(self):
    msgs = []
    cb = message_callback.list_message_callback(msgs)
    cb(["line 1", "line 2"])
    cb(["line 3"])

    self.assertSequenceEqual(msgs, ("line 1", "line 2", "line 3"))

  def test_not_empty(self):
    msgs = ["initial", "content"]
    cb = message_callback.list_message_callback(msgs)
    cb(["line 1", "line 2"])
    cb(["line 3"])

    self.assertSequenceEqual(
        msgs, ("initial", "content", "line 1", "line 2", "line 3")
    )


if __name__ == "__main__":
  absltest.main()
