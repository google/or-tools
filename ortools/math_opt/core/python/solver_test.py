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
import threading
from typing import Callable, Optional, Sequence
from absl.testing import absltest
from absl.testing import parameterized
from pybind11_abseil.status import StatusNotOk
from ortools.math_opt import callback_pb2
from ortools.math_opt import model_parameters_pb2
from ortools.math_opt import model_pb2
from ortools.math_opt import model_update_pb2
from ortools.math_opt import parameters_pb2
from ortools.math_opt import result_pb2
from ortools.math_opt.core.python import solver


def _build_simple_model() -> model_pb2.ModelProto:
    model = model_pb2.ModelProto()
    model.variables.ids.append(0)
    model.variables.lower_bounds.append(1.0)
    model.variables.upper_bounds.append(2.0)
    model.variables.integers.append(False)
    model.variables.names.append("x")
    model.objective.maximize = True
    model.objective.linear_coefficients.ids.append(0)
    model.objective.linear_coefficients.values.append(1.0)
    return model


def _solve_model(
    model: model_pb2.ModelProto,
    *,
    use_solver_class: bool,
    solver_type: parameters_pb2.SolverTypeProto = parameters_pb2.SOLVER_TYPE_GLOP,
    solver_initializer: parameters_pb2.SolverInitializerProto = parameters_pb2.SolverInitializerProto(),
    parameters: parameters_pb2.SolveParametersProto = parameters_pb2.SolveParametersProto(),
    model_parameters: model_parameters_pb2.ModelSolveParametersProto = model_parameters_pb2.ModelSolveParametersProto(),
    message_callback: Optional[Callable[[Sequence[str]], None]] = None,
    callback_registration: callback_pb2.CallbackRegistrationProto = callback_pb2.CallbackRegistrationProto(),
    user_cb: Optional[
        Callable[[callback_pb2.CallbackDataProto], callback_pb2.CallbackResultProto]
    ] = None,
    interrupter: Optional[solver.SolveInterrupter] = None,
) -> result_pb2.SolveResultProto:
    """Convenience function for both types of solve with parameter defaults."""
    if use_solver_class:
        pybind_solver = solver.new(
            solver_type,
            model,
            solver_initializer,
        )
        return pybind_solver.solve(
            parameters,
            model_parameters,
            message_callback,
            callback_registration,
            user_cb,
            interrupter,
        )
    else:
        return solver.solve(
            model,
            solver_type,
            solver_initializer,
            parameters,
            model_parameters,
            message_callback,
            callback_registration,
            user_cb,
            interrupter,
        )


class PybindSolverTest(parameterized.TestCase):

    def tearDown(self):
        super().tearDown()
        self.assertEqual(solver.debug_num_solver(), 0)

    @parameterized.named_parameters(
        dict(testcase_name="without_solver", use_solver_class=False),
        dict(testcase_name="with_solver", use_solver_class=True),
    )
    def test_valid_solve(self, use_solver_class: bool) -> None:
        model = _build_simple_model()
        result = _solve_model(model, use_solver_class=use_solver_class)
        self.assertEqual(
            result.termination.reason, result_pb2.TERMINATION_REASON_OPTIMAL
        )
        self.assertTrue(result.solutions)
        self.assertAlmostEqual(result.solutions[0].primal_solution.objective_value, 2.0)

    @parameterized.named_parameters(
        dict(testcase_name="without_solver", use_solver_class=False),
        dict(testcase_name="with_solver", use_solver_class=True),
    )
    def test_invalid_input_throws_error(self, use_solver_class: bool) -> None:
        model = _build_simple_model()
        # Add invalid variable id to cause MathOpt model validation error.
        model.objective.linear_coefficients.ids.append(7)
        model.objective.linear_coefficients.values.append(2.0)
        if sys.platform in ("darwin", "win32"):
            with self.assertRaisesRegex(RuntimeError, "id 7 not found"):
                _solve_model(model, use_solver_class=use_solver_class)
        else:
            with self.assertRaisesRegex(StatusNotOk, "id 7 not found"):
                _solve_model(model, use_solver_class=use_solver_class)

    @parameterized.named_parameters(
        dict(testcase_name="without_solver", use_solver_class=False),
        dict(testcase_name="with_solver", use_solver_class=True),
    )
    def test_solve_interrupter_interrupts_solve(self, use_solver_class: bool) -> None:
        model = _build_simple_model()
        interrupter = solver.SolveInterrupter()
        interrupter.interrupt()
        result = _solve_model(
            model, use_solver_class=use_solver_class, interrupter=interrupter
        )
        self.assertEqual(
            result.termination.reason,
            result_pb2.TERMINATION_REASON_NO_SOLUTION_FOUND,
        )
        self.assertEqual(result.termination.limit, result_pb2.LIMIT_INTERRUPTED)

    @parameterized.named_parameters(
        dict(testcase_name="without_solver", use_solver_class=False),
        dict(testcase_name="with_solver", use_solver_class=True),
    )
    def test_message_callback_is_invoked(self, use_solver_class: bool) -> None:
        model = _build_simple_model()
        messages = []
        # Message callback extends `messages` with solver output.
        _solve_model(
            model,
            use_solver_class=use_solver_class,
            parameters=parameters_pb2.SolveParametersProto(
                enable_output=True, threads=1
            ),
            message_callback=messages.extend,
        )
        self.assertIn("status:", "\n".join(messages))

    @parameterized.named_parameters(
        dict(testcase_name="without_solver", use_solver_class=False),
        dict(testcase_name="with_solver", use_solver_class=True),
    )
    def test_user_callback_is_invoked(self, use_solver_class: bool) -> None:
        model = _build_simple_model()
        solution_values = []
        mutex = threading.Lock()

        # Callback stores solution values found during solve in `solution_values`.
        def collect_solution_values_user_callback(
            cb_data: callback_pb2.CallbackDataProto,
        ) -> callback_pb2.CallbackResultProto:
            with mutex:
                assert cb_data.event == callback_pb2.CALLBACK_EVENT_MIP_SOLUTION
                solution_values.extend(cb_data.primal_solution_vector.values)
                return callback_pb2.CallbackResultProto()

        # This implicitly tests that the GIL is released, since the solve below can
        # deadlock otherwise.
        result = _solve_model(
            model,
            use_solver_class=use_solver_class,
            solver_type=parameters_pb2.SOLVER_TYPE_CP_SAT,
            callback_registration=callback_pb2.CallbackRegistrationProto(
                request_registration=[callback_pb2.CALLBACK_EVENT_MIP_SOLUTION]
            ),
            user_cb=collect_solution_values_user_callback,
        )
        self.assertEqual(
            result.termination.reason, result_pb2.TERMINATION_REASON_OPTIMAL
        )
        # `solution_values` should at least contain the optimal value 2.0.
        self.assertContainsSubset(solution_values, [2.0])

    @parameterized.named_parameters(
        dict(testcase_name="without_solver", use_solver_class=False),
        dict(testcase_name="with_solver", use_solver_class=True),
    )
    def test_solution_hint_is_used(self, use_solver_class: bool) -> None:
        model = _build_simple_model()
        solution_hint = model_parameters_pb2.SolutionHintProto()
        solution_hint.variable_values.ids.append(0)
        solution_hint.variable_values.values.append(1.0)
        # Limit the solver so that it does not find a solution other than provided.
        result = _solve_model(
            model,
            use_solver_class=use_solver_class,
            solver_type=parameters_pb2.SOLVER_TYPE_GSCIP,
            parameters=parameters_pb2.SolveParametersProto(
                node_limit=0,
                heuristics=parameters_pb2.EMPHASIS_OFF,
                presolve=parameters_pb2.EMPHASIS_OFF,
            ),
            model_parameters=model_parameters_pb2.ModelSolveParametersProto(
                solution_hints=[solution_hint]
            ),
        )
        self.assertEqual(
            result.termination.reason, result_pb2.TERMINATION_REASON_FEASIBLE
        )
        self.assertTrue(result.solutions)
        self.assertAlmostEqual(result.solutions[0].primal_solution.objective_value, 1.0)

    def test_debug_num_solver(self) -> None:
        self.assertEqual(solver.debug_num_solver(), 0)
        pybind_solver = solver.new(
            parameters_pb2.SOLVER_TYPE_GLOP,
            model_pb2.ModelProto(),
            parameters_pb2.SolverInitializerProto(),
        )
        self.assertEqual(solver.debug_num_solver(), 1)
        del pybind_solver
        self.assertEqual(solver.debug_num_solver(), 0)

    def test_incremental_solver_update(self) -> None:
        model = _build_simple_model()
        incremental_solver = solver.new(
            parameters_pb2.SOLVER_TYPE_GLOP,
            model,
            parameters_pb2.SolverInitializerProto(),
        )
        result = incremental_solver.solve(
            parameters_pb2.SolveParametersProto(),
            model_parameters_pb2.ModelSolveParametersProto(),
            None,
            callback_pb2.CallbackRegistrationProto(),
            None,
            None,
        )
        self.assertEqual(
            result.termination.reason, result_pb2.TERMINATION_REASON_OPTIMAL
        )
        self.assertAlmostEqual(result.solve_stats.best_primal_bound, 2.0)
        update = model_update_pb2.ModelUpdateProto()
        update.variable_updates.upper_bounds.ids.append(0)
        update.variable_updates.upper_bounds.values.append(2.5)
        self.assertTrue(incremental_solver.update(update))
        result = incremental_solver.solve(
            parameters_pb2.SolveParametersProto(),
            model_parameters_pb2.ModelSolveParametersProto(),
            None,
            callback_pb2.CallbackRegistrationProto(),
            None,
            None,
        )
        self.assertEqual(
            result.termination.reason, result_pb2.TERMINATION_REASON_OPTIMAL
        )
        self.assertAlmostEqual(result.solve_stats.best_primal_bound, 2.5)


class PybindSolveInterrupterTest(parameterized.TestCase):

    def test_solve_interrupter_is_interrupted(self) -> None:
        interrupter = solver.SolveInterrupter()
        self.assertFalse(interrupter.is_interrupted())
        interrupter.interrupt()
        self.assertTrue(interrupter.is_interrupted())
        del interrupter


if __name__ == "__main__":
    absltest.main()
