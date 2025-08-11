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
from ortools.util.python import pybind_solve_interrupter
from ortools.util.python import pybind_solve_interrupter_testing


class PybindPySolveInterrupterTest(absltest.TestCase):
    """Test pybind_solve_interrupter.PySolveInterrupter class.

    This tests how pybind_solve_interrupter.PySolveInterrupter is accepted by
    other
    pybind-generated APIs in Python, using pybind_solve_interrupter_testing to do
    so.
    """

    def test_none_interrupter(self) -> None:
        """Test that a `const PySolveInterrupter*` can receive nullptr."""
        # Here IsInterrupted() is a Clif wrapped C++ function that expects a `const
        # PySolveInterrupter*`. It returns None (std::nullopt) for nullptr
        # input.
        self.assertIsNone(pybind_solve_interrupter_testing.IsInterrupted(None))

    def test_untriggered_interrupter(self) -> None:
        """Test that an untriggered interrupter is properly passed."""
        interrupter = pybind_solve_interrupter.PySolveInterrupter()
        # Test the getter.
        self.assertFalse(interrupter.interrupted)
        # Test using a Clif wrapped C++ function. We test it is not None to make
        # sure Clif passes a non-null pointer to the C++ code. We thus have to use
        # assertIs() instead of assertFalse().
        self.assertIs(
            pybind_solve_interrupter_testing.IsInterrupted(interrupter), False
        )

    def test_triggered_interrupter(self) -> None:
        """Test that an untriggered interrupter is properly passed."""
        interrupter = pybind_solve_interrupter.PySolveInterrupter()
        interrupter.interrupt()
        # Test the getter.
        self.assertTrue(interrupter.interrupted)
        # Test using a Clif wrapped C++ function.
        self.assertTrue(pybind_solve_interrupter_testing.IsInterrupted(interrupter))

    def test_shared_reference(self) -> None:
        """Test that taking a std::shared_ptr<PySolveInterrupter> works."""
        # Create an interrupter and a PySolveInterrupterReference class which
        # constructor takes and std::shared_ptr<PySolveInterrupter>. We expect
        # that only one instance of the C++ PySolveInterrupter will exist here.
        interrupter = pybind_solve_interrupter.PySolveInterrupter()
        interrupter_ref = pybind_solve_interrupter_testing.PySolveInterrupterReference(
            interrupter
        )

        # Validate that we have the expected number of references of the shared_ptr
        # hold by PySolveInterrupterReference.
        self.assertEqual(interrupter_ref.use_count, 2)
        self.assertFalse(interrupter_ref.is_interrupted)

        # Triggering the interrupter should be visible the pointed
        # PySolveInterrupter in C++.
        interrupter.interrupt()
        self.assertEqual(interrupter_ref.use_count, 2)
        self.assertTrue(interrupter_ref.is_interrupted)

        # Removing the Python `interrupter` reference should make `interrupter_ref`
        # the only object that holds an std::shared_ptr on the interrupter.
        del interrupter
        self.assertEqual(interrupter_ref.use_count, 1)
        self.assertTrue(interrupter_ref.is_interrupted)

    def test_add_target(self) -> None:
        source = pybind_solve_interrupter.PySolveInterrupter()
        target = pybind_solve_interrupter.PySolveInterrupter()
        source.add_trigger_target(target)
        source.interrupt()
        self.assertTrue(pybind_solve_interrupter_testing.IsInterrupted(source))
        self.assertTrue(pybind_solve_interrupter_testing.IsInterrupted(target))

    def test_remove_existing_target(self) -> None:
        source = pybind_solve_interrupter.PySolveInterrupter()
        target = pybind_solve_interrupter.PySolveInterrupter()
        source.add_trigger_target(target)
        source.remove_trigger_target(target)
        source.interrupt()
        self.assertTrue(pybind_solve_interrupter_testing.IsInterrupted(source))
        self.assertFalse(pybind_solve_interrupter_testing.IsInterrupted(target))

    def test_remove_existing_target_added_twice(self) -> None:
        source = pybind_solve_interrupter.PySolveInterrupter()
        target = pybind_solve_interrupter.PySolveInterrupter()
        source.add_trigger_target(target)
        source.add_trigger_target(target)
        source.remove_trigger_target(target)
        source.interrupt()
        self.assertTrue(pybind_solve_interrupter_testing.IsInterrupted(source))
        self.assertFalse(pybind_solve_interrupter_testing.IsInterrupted(target))

    def test_dead_target(self) -> None:
        source = pybind_solve_interrupter.PySolveInterrupter()
        target = pybind_solve_interrupter.PySolveInterrupter()
        source.add_trigger_target(target)
        del target
        source.interrupt()
        self.assertTrue(pybind_solve_interrupter_testing.IsInterrupted(source))

    def test_remove_existing_target_twice(self) -> None:
        source = pybind_solve_interrupter.PySolveInterrupter()
        target = pybind_solve_interrupter.PySolveInterrupter()
        source.add_trigger_target(target)
        source.remove_trigger_target(target)
        source.remove_trigger_target(target)
        source.interrupt()
        self.assertTrue(pybind_solve_interrupter_testing.IsInterrupted(source))
        self.assertFalse(pybind_solve_interrupter_testing.IsInterrupted(target))

    def test_remove_non_target(self) -> None:
        source = pybind_solve_interrupter.PySolveInterrupter()
        target = pybind_solve_interrupter.PySolveInterrupter()
        source.remove_trigger_target(target)
        source.interrupt()
        self.assertTrue(pybind_solve_interrupter_testing.IsInterrupted(source))
        self.assertFalse(pybind_solve_interrupter_testing.IsInterrupted(target))


if __name__ == "__main__":
    absltest.main()
