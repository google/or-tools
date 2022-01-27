// Copyright 2010-2021 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ortools/sat/table.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/base/int_type.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

std::function<void(Model*)> LiteralTableConstraint(
    const std::vector<std::vector<Literal>>& literal_tuples,
    const std::vector<Literal>& line_literals) {
  return [=](Model* model) {
    CHECK_EQ(literal_tuples.size(), line_literals.size());
    const int num_tuples = line_literals.size();
    if (num_tuples == 0) return;
    const int tuple_size = literal_tuples[0].size();
    if (tuple_size == 0) return;
    for (int i = 1; i < num_tuples; ++i) {
      CHECK_EQ(tuple_size, literal_tuples[i].size());
    }

    absl::flat_hash_map<LiteralIndex, std::vector<LiteralIndex>>
        line_literals_per_literal;
    for (int i = 0; i < num_tuples; ++i) {
      const LiteralIndex selected_index = line_literals[i].Index();
      for (const Literal l : literal_tuples[i]) {
        line_literals_per_literal[l.Index()].push_back(selected_index);
      }
    }

    // line_literals[i] == true => literal_tuples[i][j] == true.
    // literal_tuples[i][j] == false => line_literals[i] == false.
    for (int i = 0; i < num_tuples; ++i) {
      const Literal line_is_selected = line_literals[i];
      for (const Literal lit : literal_tuples[i]) {
        model->Add(Implication(line_is_selected, lit));
      }
    }

    // Exactly one selected literal is true.
    model->Add(ExactlyOneConstraint(line_literals));

    // If all selected literals of the lines containing a literal are false,
    // then the literal is false.
    for (const auto& p : line_literals_per_literal) {
      std::vector<Literal> clause;
      for (const auto& index : p.second) {
        clause.push_back(Literal(index));
      }
      clause.push_back(Literal(p.first).Negated());
      model->Add(ClauseConstraint(clause));
    }
  };
}

}  // namespace sat
}  // namespace operations_research
