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

from absl.testing import absltest
from ortools.util.python import pybind_solve_interrupter_testing
from ortools.util.python import solve_interrupter


class SolveInterrupterTest(absltest.TestCase):

    def test_untriggered_interrupter(self) -> None:
        interrupter = solve_interrupter.SolveInterrupter()
        self.assertFalse(interrupter.interrupted)

    def test_triggered_interrupter(self) -> None:
        interrupter = solve_interrupter.SolveInterrupter()
        interrupter.interrupt()
        self.assertTrue(interrupter.interrupted)

    def test_add_target(self) -> None:
        source = solve_interrupter.SolveInterrupter()
        target = solve_interrupter.SolveInterrupter()
        source.add_trigger_target(target)
        source.interrupt()
        self.assertTrue(source.interrupted)
        self.assertTrue(target.interrupted)

    def test_remove_existing_target(self) -> None:
        source = solve_interrupter.SolveInterrupter()
        target = solve_interrupter.SolveInterrupter()
        source.add_trigger_target(target)
        source.remove_trigger_target(target)
        source.interrupt()
        self.assertTrue(source.interrupted)
        self.assertFalse(target.interrupted)

    def test_remove_existing_target_added_twice(self) -> None:
        source = solve_interrupter.SolveInterrupter()
        target = solve_interrupter.SolveInterrupter()
        source.add_trigger_target(target)
        source.add_trigger_target(target)
        source.remove_trigger_target(target)
        source.interrupt()
        self.assertTrue(source.interrupted)
        self.assertFalse(target.interrupted)

    def test_dead_target(self) -> None:
        source = solve_interrupter.SolveInterrupter()
        target = solve_interrupter.SolveInterrupter()
        source.add_trigger_target(target)
        del target
        source.interrupt()
        self.assertTrue(source.interrupted)

    def test_remove_existing_target_twice(self) -> None:
        source = solve_interrupter.SolveInterrupter()
        target = solve_interrupter.SolveInterrupter()
        source.add_trigger_target(target)
        source.remove_trigger_target(target)
        source.remove_trigger_target(target)
        source.interrupt()
        self.assertTrue(source.interrupted)
        self.assertFalse(target.interrupted)

    def test_remove_non_target(self) -> None:
        source = solve_interrupter.SolveInterrupter()
        target = solve_interrupter.SolveInterrupter()
        source.remove_trigger_target(target)
        source.interrupt()
        self.assertTrue(source.interrupted)
        self.assertFalse(target.interrupted)

    def test_callback_already_interrupted(self) -> None:
        num_calls = 0

        def callback():
            nonlocal num_calls
            num_calls += 1

        interrupter = solve_interrupter.SolveInterrupter()
        interrupter.interrupt()

        with interrupter.interruption_callback(callback):
            self.assertEqual(num_calls, 1)

        self.assertEqual(num_calls, 1)

    def test_callback_interruption(self) -> None:
        num_calls = 0

        def callback():
            nonlocal num_calls
            num_calls += 1

        interrupter = solve_interrupter.SolveInterrupter()

        with interrupter.interruption_callback(callback):
            self.assertEqual(num_calls, 0)
            interrupter.interrupt()
            self.assertEqual(num_calls, 1)

        self.assertEqual(num_calls, 1)

    def test_callback_interruption_after_removal(self) -> None:
        num_calls = 0

        def callback():
            nonlocal num_calls
            num_calls += 1

        interrupter = solve_interrupter.SolveInterrupter()

        with interrupter.interruption_callback(callback):
            self.assertEqual(num_calls, 0)

        interrupter.interrupt()
        self.assertEqual(num_calls, 0)

    def test_callback_nointerruption(self) -> None:
        num_calls = 0

        def callback():
            nonlocal num_calls
            num_calls += 1

        interrupter = solve_interrupter.SolveInterrupter()

        with interrupter.interruption_callback(callback):
            self.assertEqual(num_calls, 0)

        self.assertEqual(num_calls, 0)

    def test_callback_with_exception_in_callback(self) -> None:
        num_calls = 0

        def callback():
            nonlocal num_calls
            num_calls += 1
            raise ValueError("error-in-callback")

        interrupter = solve_interrupter.SolveInterrupter()

        # has_finished is set to true after the call to interrupter.interrupt() and
        # will be used to validate that interrupt() does not raise the exception,
        # only the __exit__() of interruption_callback() context does.
        has_finished = False
        with self.assertRaises(solve_interrupter.CallbackError) as cm:
            with interrupter.interruption_callback(callback):
                before_interrupt_num_calls = num_calls
                interrupter.interrupt()
                after_interrupt_num_calls = num_calls
                has_finished = True
        #  Test the cause of the CallbackError, which should be the original error.
        self.assertIsInstance(cm.exception.__cause__, ValueError)
        self.assertEqual(str(cm.exception.__cause__), "error-in-callback")

        self.assertEqual(before_interrupt_num_calls, 0)
        self.assertEqual(after_interrupt_num_calls, 1)
        self.assertTrue(has_finished)
        self.assertEqual(num_calls, 1)

    def test_callback_with_exception_in_context(self) -> None:
        num_calls = 0

        def callback():
            nonlocal num_calls
            num_calls += 1

        interrupter = solve_interrupter.SolveInterrupter()

        with self.assertRaisesRegex(ValueError, "error-in-context"):
            with interrupter.interruption_callback(callback):
                raise ValueError("error-in-context")

        interrupter.interrupt()
        self.assertEqual(num_calls, 0)

    def test_callback_with_exception_in_callback_and_context(self) -> None:
        num_calls = 0

        def callback():
            nonlocal num_calls
            num_calls += 1
            raise ValueError("error-in-callback")

        interrupter = solve_interrupter.SolveInterrupter()

        with self.assertRaisesRegex(ValueError, "error-in-context"):
            with interrupter.interruption_callback(callback):
                before_interrupt_num_calls = num_calls
                interrupter.interrupt()
                after_interrupt_num_calls = num_calls
                raise ValueError("error-in-context")

        self.assertEqual(before_interrupt_num_calls, 0)
        self.assertEqual(after_interrupt_num_calls, 1)
        self.assertEqual(num_calls, 1)

    def test_pybind_interrupter(self) -> None:
        interrupter = solve_interrupter.SolveInterrupter()
        pybind_interrupter = interrupter.pybind_interrupter

        self.assertFalse(
            pybind_solve_interrupter_testing.IsInterrupted(pybind_interrupter)
        )

        interrupter.interrupt()

        self.assertTrue(
            pybind_solve_interrupter_testing.IsInterrupted(pybind_interrupter)
        )


if __name__ == "__main__":
    absltest.main()
