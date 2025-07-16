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
from ortools.sat import cp_model_pb2
from ortools.sat.python import cp_model_builder


class CpModelBuilderTest(absltest.TestCase):

    def test_basic(self):
        model_proto = cp_model_builder.CpModelProto()

        # Singular message.
        objective = model_proto.objective

        # Singular int.
        self.assertEqual(objective.offset, 0)
        objective.offset = 123
        self.assertEqual(objective.offset, 123)

        # Set a message.
        new_obj = cp_model_builder.CpObjectiveProto()
        new_obj.offset = 456
        model_proto.objective = new_obj
        self.assertEqual(objective.offset, 456)

        # Large int.
        objective.offset = 500000000000
        self.assertEqual(objective.offset, 500000000000)

        # Repeated message.
        my_var = model_proto.variables.add()

        # Singular string.
        self.assertEqual(my_var.name, "")
        my_var.name = "my_var"
        self.assertEqual(my_var.name, "my_var")
        my_var.domain.extend([0, 1])
        domain = list(my_var.domain)
        self.assertLen(domain, 2)
        self.assertEqual(domain[0], 0)
        self.assertEqual(domain[1], 1)

        # Repeated int.
        objective.vars.append(0)
        self.assertLen(objective.vars, 1)
        self.assertEqual(objective.vars[0], 0)
        objective.vars[0] = 42
        self.assertEqual(objective.vars[0], 42)

        # Singular enum
        search_strategy = model_proto.search_strategy.add()
        self.assertEqual(
            search_strategy.variable_selection_strategy,
            cp_model_builder.DecisionStrategyProto.CHOOSE_FIRST,
        )
        search_strategy.variable_selection_strategy = (
            cp_model_builder.DecisionStrategyProto.CHOOSE_LOWEST_MIN
        )
        self.assertEqual(
            search_strategy.variable_selection_strategy,
            cp_model_pb2.DecisionStrategyProto.CHOOSE_LOWEST_MIN,
        )


if __name__ == "__main__":
    absltest.main()
