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

"""Definition and tools for message callbacks.

Message callbacks are used to get the text messages emitted by solvers during
the solve.

  Typical usage example:

  # Print messages to stdout.
  result = solve.solve(
      model, parameters.SolverType.GSCIP,
      msg_cb=message_callback.printer_message_callback(prefix='[solver] '))

  # Log messages with absl.logging.
  result = solve.solve(
      model, parameters.SolverType.GSCIP,
      msg_cb=lambda msgs: message_callback.log_messages(
          msgs, prefix='[solver] '))
"""

import sys
import threading
from typing import Callable, List, Sequence, TextIO

from absl import logging

SolveMessageCallback = Callable[[Sequence[str]], None]


def printer_message_callback(
    *, file: TextIO = sys.stdout, prefix: str = ""
) -> SolveMessageCallback:
    """Returns a message callback that prints to a file.

    It prints its output to the given text file, prefixing each line with the
    given prefix.

    For each call to the returned message callback, the output_stream is flushed.

    Args:
      file: The file to print to. It prints to stdout by default.
      prefix: The prefix to print in front of each line.

    Returns:
      A function matching the expected signature for message callbacks.
    """
    mutex = threading.Lock()

    def callback(messages: Sequence[str]) -> None:
        with mutex:
            for message in messages:
                file.write(prefix)
                file.write(message)
                file.write("\n")
            file.flush()

    return callback


def log_messages(
    messages: Sequence[str], *, level: int = logging.INFO, prefix: str = ""
) -> None:
    """Logs the input messages from a message callback using absl.logging.log().

    It logs each line with the given prefix. It setups absl.logging so that the
    logs use the file name and line of the caller of this function.

      Typical usage example:

      result = solve.solve(
          model, parameters.SolverType.GSCIP,
          msg_cb=lambda msgs: message_callback.log_messages(
              msgs, prefix='[solver] '))

    Args:
      messages: The messages received in the message callback (typically a lambda
        function in the caller code).
      level: One of absl.logging.(DEBUG|INFO|WARNING|ERROR|FATAL).
      prefix: The prefix to print in front of each line.
    """
    for message in messages:
        logging.log(level, "%s%s", prefix, message)


logging.ABSLLogger.register_frame_to_skip(__file__, log_messages.__name__)


def vlog_messages(messages: Sequence[str], level: int, *, prefix: str = "") -> None:
    """Logs the input messages from a message callback using absl.logging.vlog().

    It logs each line with the given prefix. It setups absl.logging so that the
    logs use the file name and line of the caller of this function.

      Typical usage example:

      result = solve.solve(
          model, parameters.SolverType.GSCIP,
          msg_cb=lambda msgs: message_callback.vlog_messages(
              msgs, 1, prefix='[solver] '))

    Args:
      messages: The messages received in the message callback (typically a lambda
        function in the caller code).
      level: The verbose log level, e.g. 1, 2...
      prefix: The prefix to print in front of each line.
    """
    for message in messages:
        logging.vlog(level, "%s%s", prefix, message)


logging.ABSLLogger.register_frame_to_skip(__file__, vlog_messages.__name__)


def list_message_callback(sink: List[str]) -> SolveMessageCallback:
    """Returns a message callback that logs messages to a list.

    Args:
      sink: The list to append messages to.

    Returns:
      A function matching the expected signature for message callbacks.
    """
    mutex = threading.Lock()

    def callback(messages: Sequence[str]) -> None:
        with mutex:
            for message in messages:
                sink.append(message)

    return callback
