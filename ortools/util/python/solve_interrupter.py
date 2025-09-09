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

"""Python interrupter for solves."""

from collections.abc import Callable, Iterator
import contextlib
from typing import Optional

from absl import logging

from ortools.util.python import pybind_solve_interrupter


class CallbackError(Exception):
    """Exception raised when an interrupter callback fails.

    When using SolveInterrupter.interruption_callback(), this exception is raised
    when exiting the context manager if the callback failed. The error in the
    callback is the cause of this exception.
    """


class SolveInterrupter:
    """Interrupter used by solvers to know when they should interrupt the solve.

    Once triggered with interrupt(), an interrupter can't be reset. It can be
    triggered from any thread.

    Thread-safety: APIs on this class are safe to call concurrently from multiple
    threads.

    Attributes:
      pybind_interrupter: The pybind wrapper around PySolveInterrupter.
    """

    def __init__(self) -> None:
        self.pybind_interrupter = pybind_solve_interrupter.PySolveInterrupter()

    def interrupt(self) -> None:
        """Interrupts the solve as soon as possible.

        Once requested the interruption can't be reset. The user should use a new
        SolveInterrupter for later solves.

        It is safe to call this function multiple times. Only the first call will
        have visible effects; other calls will be ignored.
        """
        self.pybind_interrupter.interrupt()

    @property
    def interrupted(self) -> bool:
        """True if the solve interruption has been requested."""
        return self.pybind_interrupter.interrupted

    def add_trigger_target(self, target: "SolveInterrupter") -> None:
        """Triggers the target when this interrupter is triggered."""
        self.pybind_interrupter.add_trigger_target(target.pybind_interrupter)

    def remove_trigger_target(self, target: "SolveInterrupter") -> None:
        """Removes the target if not null and present, else do nothing."""
        self.pybind_interrupter.remove_trigger_target(target.pybind_interrupter)

    @contextlib.contextmanager
    def interruption_callback(self, callback: Callable[[], None]) -> Iterator[None]:
        """Returns a context manager that (un)register the provided callback.

        The callback is immediately called if the interrupter has already been
        triggered or if it is triggered during the registration. This is typically
        useful for a solver implementation so that it does not have to test
        `interrupted` to do the same thing it does in the callback. Simply
        registering the callback is enough.

        The callback function can't make calls to interruption_callback(), and
        interrupt(). This would result is a deadlock. Reading `interrupted` is fine
        though.

        Exceptions raised in the callback are raised on exit from the context
        manager if no other error happens within the context. Else the exception is
        logged.

        Args:
          callback: The callback.

        Returns:
          A context manager.

        Raises:
          CallbackError: When exiting the context manager if an exception was raised
            in the callback.
        """
        callback_error: Optional[Exception] = None

        def protetected_callback():
            """Calls callback() storing any exception in callback_error."""
            nonlocal callback_error
            try:
                callback()
            except Exception as e:  # pylint: disable=broad-exception-caught
                # It is fine to set callback_error without any threading protection as
                # the SolveInterrupter guarantees it will only call it at most once.
                callback_error = e

        callback_id = self.pybind_interrupter.add_interruption_callback(
            protetected_callback
        )

        no_exception_in_context = False
        try:
            yield
            no_exception_in_context = True
        finally:
            self.pybind_interrupter.remove_interruption_callback(callback_id)
            # It is fine to access callback_error without threading protection after
            # remove_interruption_callback() has returned as the SolveInterrupter
            # guarantees any pending call is done and no future call can happen.
            if callback_error is not None:
                if no_exception_in_context:
                    raise CallbackError() from callback_error
                # We don't want the error in the context to be masked by an error in the
                # callback. We log it instead.
                logging.error(
                    "An exception occurred in callback but is masked by another"
                    " exception: %s",
                    repr(callback_error),
                )
