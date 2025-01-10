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

import datetime
from typing import Any

from absl.testing import absltest
from absl.testing import parameterized
from ortools.pdlp import solvers_pb2 as pdlp_solvers_pb2
from ortools.glop import parameters_pb2 as glop_parameters_pb2
from ortools.gscip import gscip_pb2
from ortools.math_opt import parameters_pb2 as math_opt_parameters_pb2
from ortools.math_opt.python import parameters
from ortools.math_opt.python.testing import compare_proto
from ortools.math_opt.solvers import glpk_pb2
from ortools.math_opt.solvers import gurobi_pb2
from ortools.math_opt.solvers import highs_pb2
from ortools.math_opt.solvers import osqp_pb2
from ortools.sat import sat_parameters_pb2


class GurobiParameters(absltest.TestCase):

    def test_to_proto(self) -> None:
        gurobi_proto = parameters.GurobiParameters(
            param_values={"x": "dog", "ab": "7"}
        ).to_proto()
        expected_proto = gurobi_pb2.GurobiParametersProto(
            parameters=[
                gurobi_pb2.GurobiParametersProto.Parameter(name="x", value="dog"),
                gurobi_pb2.GurobiParametersProto.Parameter(name="ab", value="7"),
            ]
        )
        self.assertEqual(expected_proto, gurobi_proto)


class GlpkParameters(absltest.TestCase):

    def test_to_proto(self) -> None:
        # Test with `optional bool` set to true.
        glpk_proto = parameters.GlpkParameters(
            compute_unbound_rays_if_possible=True
        ).to_proto()
        expected_proto = glpk_pb2.GlpkParametersProto(
            compute_unbound_rays_if_possible=True
        )
        self.assertEqual(glpk_proto, expected_proto)

        # Test with `optional bool` set to false.
        glpk_proto = parameters.GlpkParameters(
            compute_unbound_rays_if_possible=False
        ).to_proto()
        expected_proto = glpk_pb2.GlpkParametersProto(
            compute_unbound_rays_if_possible=False
        )
        self.assertEqual(glpk_proto, expected_proto)

        # Test with `optional bool` unset.
        glpk_proto = parameters.GlpkParameters().to_proto()
        expected_proto = glpk_pb2.GlpkParametersProto()
        self.assertEqual(glpk_proto, expected_proto)


class ProtoRoundTrip(absltest.TestCase):

    def test_solver_type_round_trip(self) -> None:
        for solver_type in parameters.SolverType:
            self.assertEqual(
                solver_type,
                parameters.solver_type_from_proto(
                    parameters.solver_type_to_proto(solver_type)
                ),
            )
        self.assertEqual(
            math_opt_parameters_pb2.SOLVER_TYPE_UNSPECIFIED,
            parameters.solver_type_to_proto(None),
        )
        self.assertIsNone(
            parameters.solver_type_from_proto(
                math_opt_parameters_pb2.SOLVER_TYPE_UNSPECIFIED
            )
        )

    def test_lp_algorithm_round_trip(self) -> None:
        for lp_alg in parameters.LPAlgorithm:
            self.assertEqual(
                lp_alg,
                parameters.lp_algorithm_from_proto(
                    parameters.lp_algorithm_to_proto(lp_alg)
                ),
            )
        self.assertEqual(
            math_opt_parameters_pb2.LP_ALGORITHM_UNSPECIFIED,
            parameters.lp_algorithm_to_proto(None),
        )
        self.assertIsNone(
            parameters.lp_algorithm_from_proto(
                math_opt_parameters_pb2.LP_ALGORITHM_UNSPECIFIED
            )
        )

    def test_emphasis_round_trip(self) -> None:
        for emph in parameters.Emphasis:
            self.assertEqual(
                emph,
                parameters.emphasis_from_proto(parameters.emphasis_to_proto(emph)),
            )
        self.assertEqual(
            math_opt_parameters_pb2.EMPHASIS_UNSPECIFIED,
            parameters.emphasis_to_proto(None),
        )
        self.assertIsNone(
            parameters.emphasis_from_proto(math_opt_parameters_pb2.EMPHASIS_UNSPECIFIED)
        )


class SolveParametersTest(compare_proto.MathOptProtoAssertions, parameterized.TestCase):
    """Test case for tests of SolveParameters."""

    def test_common_to_proto(self) -> None:
        params = parameters.SolveParameters(
            time_limit=datetime.timedelta(seconds=10),
            iteration_limit=7,
            node_limit=3,
            cutoff_limit=9.5,
            objective_limit=10.5,
            best_bound_limit=11.5,
            solution_limit=2,
            enable_output=True,
            threads=3,
            random_seed=12,
            absolute_gap_tolerance=1.3,
            relative_gap_tolerance=0.05,
            solution_pool_size=17,
            lp_algorithm=parameters.LPAlgorithm.BARRIER,
            presolve=parameters.Emphasis.OFF,
            cuts=parameters.Emphasis.LOW,
            heuristics=parameters.Emphasis.MEDIUM,
            scaling=parameters.Emphasis.HIGH,
        )
        expected = math_opt_parameters_pb2.SolveParametersProto(
            iteration_limit=7,
            node_limit=3,
            cutoff_limit=9.5,
            objective_limit=10.5,
            best_bound_limit=11.5,
            solution_limit=2,
            enable_output=True,
            threads=3,
            random_seed=12,
            absolute_gap_tolerance=1.3,
            relative_gap_tolerance=0.05,
            solution_pool_size=17,
            lp_algorithm=math_opt_parameters_pb2.LP_ALGORITHM_BARRIER,
            presolve=math_opt_parameters_pb2.EMPHASIS_OFF,
            cuts=math_opt_parameters_pb2.EMPHASIS_LOW,
            heuristics=math_opt_parameters_pb2.EMPHASIS_MEDIUM,
            scaling=math_opt_parameters_pb2.EMPHASIS_HIGH,
        )
        expected.time_limit.FromTimedelta(datetime.timedelta(seconds=10))
        self.assert_protos_equiv(expected, params.to_proto())

    def test_to_proto_with_none(self) -> None:
        params = parameters.SolveParameters()
        expected = math_opt_parameters_pb2.SolveParametersProto()
        self.assert_protos_equiv(expected, params.to_proto())

    @parameterized.named_parameters(
        (
            "gscip",
            "gscip",
            gscip_pb2.GScipParameters(print_detailed_solving_stats=True),
        ),
        (
            "glop",
            "glop",
            glop_parameters_pb2.GlopParameters(refactorization_threshold=1e-5),
        ),
        (
            "gurobi",
            "gurobi",
            parameters.GurobiParameters(param_values={"NodeLimit": "30"}),
        ),
        (
            "cp_sat",
            "cp_sat",
            sat_parameters_pb2.SatParameters(random_branches_ratio=0.5),
        ),
        ("osqp", "osqp", osqp_pb2.OsqpSettingsProto(sigma=1.2)),
        (
            "glpk",
            "glpk",
            parameters.GlpkParameters(compute_unbound_rays_if_possible=True),
        ),
        (
            "highs",
            "highs",
            highs_pb2.HighsOptionsProto(bool_options={"solve_relaxation": True}),
        ),
    )
    def test_to_proto_with_specifics(
        self, field: str, solver_specific_param: Any
    ) -> None:
        solve_params = parameters.SolveParameters(threads=3)
        setattr(solve_params, field, solver_specific_param)
        expected = math_opt_parameters_pb2.SolveParametersProto(threads=3)
        proto_solver_specific_param = (
            solver_specific_param.to_proto()
            if field in ("gurobi", "glpk")
            else solver_specific_param
        )
        getattr(expected, field).CopyFrom(proto_solver_specific_param)
        self.assert_protos_equiv(expected, solve_params.to_proto())

    def test_to_proto_no_specifics(self) -> None:
        solve_params = parameters.SolveParameters(threads=3)
        expected = math_opt_parameters_pb2.SolveParametersProto(threads=3)
        self.assert_protos_equiv(expected, solve_params.to_proto())


if __name__ == "__main__":
    absltest.main()
