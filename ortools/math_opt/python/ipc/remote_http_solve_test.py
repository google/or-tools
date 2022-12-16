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

import requests_mock
from absl.testing import absltest
from ortools.gscip import gscip_pb2
from ortools.math_opt.python import mathopt
from ortools.math_opt.python.ipc import remote_http_solve

_MOCK_API_KEY = "1234"
_ENDPOINT = "https://optimization.googleapis.com/v1/mathopt:solveMathOptModel"

_JSON_RESPONSE = """{
  "result": {
    "termination": {
      "reason": "TERMINATION_REASON_OPTIMAL",
      "detail": "Optimization went well",
      "problemStatus": {
        "primalStatus": "FEASIBILITY_STATUS_FEASIBLE",
        "dualStatus": "FEASIBILITY_STATUS_FEASIBLE"
      },
      "objectiveBounds": {
        "primalBound": 1,
        "dualBound": 1
      }
    },
    "solutions": [
      {
        "primalSolution": {
          "variableValues": {
            "ids": [
              "0",
              "1"
            ],
            "values": [
              1,
              0
            ]
          },
          "objectiveValue": 1,
          "feasibilityStatus": "SOLUTION_STATUS_FEASIBLE"
        }
      }
    ],
    "solveStats": {
      "solveTime": "2.0s",
      "problemStatus": {
        "primalStatus": "FEASIBILITY_STATUS_FEASIBLE",
        "dualStatus": "FEASIBILITY_STATUS_FEASIBLE"
      }
    }
  },
  "messages": [
    "Logs from the solver:",
    "End of logs"
  ]
}
"""


def _simple_model() -> tuple[mathopt.Model, mathopt.Variable, mathopt.Variable]:
    mod = mathopt.Model(name="test_mod")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    mod.maximize(x - y)
    return mod, x, y


class RemoteHttpSolveTest(absltest.TestCase):

    def test_session_headers(self):
        deadline = 1.0
        server_deadline = deadline * (1 - remote_http_solve._RELATIVE_TIME_BUFFER)
        session = remote_http_solve.create_optimization_service_session(
            _MOCK_API_KEY, deadline
        )
        self.assertEqual(session.headers["Content-Type"], "application/json")
        self.assertEqual(session.headers["Connection"], "keep-alive")
        self.assertEqual(session.headers["Keep-Alive"], "timeout=1.0, max=1")
        self.assertAlmostEqual(
            float(session.headers["X-Server-Timeout"]), server_deadline, delta=1e-4
        )
        self.assertEqual(session.headers["X-Goog-Api-Key"], _MOCK_API_KEY)

    @requests_mock.Mocker()
    def test_remote_http_solve(self, m: requests_mock.Mocker):
        mod, x, y = _simple_model()
        # Mock request with the JSON response and a fake API key.
        m.post(
            _ENDPOINT,
            text=_JSON_RESPONSE,
            status_code=200,
        )

        remote_solve_result, messages = remote_http_solve.remote_http_solve(
            mod,
            mathopt.SolverType.GSCIP,
            params=mathopt.SolveParameters(enable_output=True),
            api_key=_MOCK_API_KEY,
            resources=mathopt.SolverResources(ram=1024 * 1024 * 1024),
        )

        self.assertGreaterEqual(len(remote_solve_result.solutions), 1)
        self.assertIsNotNone(remote_solve_result.solutions[0].primal_solution)
        self.assertAlmostEqual(
            remote_solve_result.solutions[0].primal_solution.objective_value,
            1.0,
            delta=1e-4,
        )
        self.assertEqual(
            remote_solve_result.solutions[0].primal_solution.variable_values[x], 1.0
        )
        self.assertEqual(
            remote_solve_result.solutions[0].primal_solution.variable_values[y], 0.0
        )
        self.assertListEqual(messages, ["Logs from the solver:", "End of logs"])

    @requests_mock.Mocker()
    def test_remote_http_solve_on_different_endpoint(self, m: requests_mock.Mocker):
        mod, _, _ = _simple_model()
        # Mock request with the JSON response and a fake API key.
        new_endpoint = "https://new.math.opt.com/v1:solve"
        m.post(
            new_endpoint,
            text=_JSON_RESPONSE,
            status_code=200,
        )

        remote_solve_result, messages = remote_http_solve.remote_http_solve(
            mod,
            mathopt.SolverType.GSCIP,
            endpoint=new_endpoint,
            api_key=_MOCK_API_KEY,
        )

        self.assertGreaterEqual(len(remote_solve_result.solutions), 1)
        self.assertIsNotNone(remote_solve_result.solutions[0].primal_solution)
        self.assertAlmostEqual(
            remote_solve_result.solutions[0].primal_solution.objective_value,
            1.0,
            delta=1e-4,
        )
        self.assertGreater(len(messages), 1)
        self.assertListEqual(messages, ["Logs from the solver:", "End of logs"])

    @requests_mock.Mocker()
    def test_server_down(self, m: requests_mock.Mocker):
        mod, _, _ = _simple_model()
        # Mock request with the JSON response and a fake API key.
        server_error_json = """{
      "error": {
        "code": 500,
        "message": "server down, sorry"}
    }
    """
        m.post(
            _ENDPOINT,
            text=server_error_json,
            status_code=500,
        )

        with self.assertRaisesRegex(
            remote_http_solve.OptimizationServiceError, "server down, sorry"
        ):
            remote_http_solve.remote_http_solve(
                mod, mathopt.SolverType.GSCIP, api_key=_MOCK_API_KEY
            )

    def test_request_serialization_error(self):
        mod, _, _ = _simple_model()
        params = mathopt.SolveParameters()
        params.gscip.heuristics = gscip_pb2.GScipParameters.MetaParamValue.AGGRESSIVE

        with self.assertRaises(ValueError):
            remote_http_solve.remote_http_solve(
                mod, mathopt.SolverType.GSCIP, params, api_key=_MOCK_API_KEY
            )

    @requests_mock.Mocker()
    def test_response_serialization_error(self, m: requests_mock.Mocker):
        mod, _, _ = _simple_model()
        # Mock request with the JSON response and a fake API key.
        invalid_json_response = """{
        "notAValidResponse" = 0.0
    }
    """
        m.post(
            _ENDPOINT,
            text=invalid_json_response,
        )

        with self.assertRaisesRegex(
            ValueError,
            "not a valid SolveMathOptModelResponse JSON",
        ):
            remote_http_solve.remote_http_solve(
                mod, mathopt.SolverType.GSCIP, api_key=_MOCK_API_KEY
            )


if __name__ == "__main__":
    absltest.main()
