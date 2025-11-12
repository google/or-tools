// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_SAT_VARIABLE_EXPAND_H_
#define ORTOOLS_SAT_VARIABLE_EXPAND_H_

#include <cstdint>
#include <functional>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/solution_crush.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

enum class ObjectiveStatus {
  kNotInObjective,
  kInObjectiveAndMinimization,
  kInObjectiveAndMaximization,
};

template <typename Sink>
void AbslStringify(Sink& sink, ObjectiveStatus obj_status) {
  switch (obj_status) {
    case ObjectiveStatus::kNotInObjective:
      sink.Append("kNotInObjective");
      return;
    case ObjectiveStatus::kInObjectiveAndMinimization:
      sink.Append("kInObjectiveAndMinimization");
      return;
    case ObjectiveStatus::kInObjectiveAndMaximization:
      sink.Append("kInObjectiveAndMaximization");
      return;
  }
}

class ValueEncoding {
 public:
  ValueEncoding(int var, PresolveContext* context,
                SolutionCrush& solution_crush);
  // Build the set of observed values.
  void AddValueToEncode(int64_t value);
  void CloseEncodedValues(ObjectiveStatus objective_status);

  // Getters on the observed values.
  bool empty() const;
  bool is_fully_encoded() const;
  const std::vector<int64_t>& encoded_values() const;

  // Setters and getters on the value encoding.
  void ForceFullEncoding();
  void CreateAllValueEncodingLiterals();
  int literal(int64_t value) const;
  const absl::btree_map<int64_t, int>& encoding() const;

 private:
  const int var_;
  const Domain var_domain_;
  PresolveContext* context_;
  SolutionCrush& solution_crush_;
  std::vector<int64_t> encoded_values_;
  absl::btree_map<int64_t, int> encoding_;
  bool is_closed_ = false;
  bool is_fully_encoded_ = false;
};

class OrderEncoding {
 public:
  OrderEncoding(int var, PresolveContext* context,
                SolutionCrush& solution_crush);
  void InsertLeLiteral(int64_t value, int literal);
  void InsertGeLiteral(int64_t value, int literal);
  void CreateAllOrderEncodingLiterals(const ValueEncoding& values);
  int ge_literal(int64_t value) const;
  int le_literal(int64_t value) const;
  int num_encoded_values() const;

 private:
  void CollectAllOrderEncodingValues();

  const int var_;
  const Domain var_domain_;
  PresolveContext* context_;
  SolutionCrush& solution_crush_;
  absl::btree_map<int64_t, absl::flat_hash_set<int>> tmp_ge_to_literals_;
  absl::btree_map<int64_t, absl::flat_hash_set<int>> tmp_le_to_literals_;
  absl::btree_map<int64_t, int> encoded_le_literal_;
};

void TryToReplaceVariableByItsEncoding(
    int var, std::function<void(ConstraintProto*)> presolve_one_constraint,
    PresolveContext* context, SolutionCrush& solution_crush);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_VARIABLE_EXPAND_H_
