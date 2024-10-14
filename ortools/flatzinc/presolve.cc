// Copyright 2010-2024 Google LLC
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

#include "ortools/flatzinc/presolve.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/flatzinc/model.h"
#include "ortools/util/logging.h"

ABSL_FLAG(bool, fz_floats_are_ints, false,
          "Interpret floats as integers in all variables and constraints.");

namespace operations_research {
namespace fz {
namespace {
enum PresolveState { ALWAYS_FALSE, ALWAYS_TRUE, UNDECIDED };

template <class T>
bool IsArrayBoolean(const std::vector<T>& values) {
  for (int i = 0; i < values.size(); ++i) {
    if (values[i] != 0 && values[i] != 1) {
      return false;
    }
  }
  return true;
}

template <class T>
bool AtMostOne0OrAtMostOne1(const std::vector<T>& values) {
  CHECK(IsArrayBoolean(values));
  int num_zero = 0;
  int num_one = 0;
  for (T val : values) {
    if (val) {
      num_one++;
    } else {
      num_zero++;
    }
    if (num_one > 1 && num_zero > 1) {
      return false;
    }
  }
  return true;
}

template <class T>
void AppendIfNotInSet(T* value, absl::flat_hash_set<T*>* s,
                      std::vector<T*>* vec) {
  if (s->insert(value).second) {
    vec->push_back(value);
  }
  DCHECK_EQ(s->size(), vec->size());
}

}  // namespace

// Note on documentation
//
// In order to document presolve rules, we will use the following naming
// convention:
//   - x, x1, xi, y, y1, yi denote integer variables
//   - b, b1, bi denote boolean variables
//   - c, c1, ci denote integer constants
//   - t, t1, ti denote boolean constants
//   - => x after a constraint denotes the target variable of this constraint.
// Arguments are listed in order.

// Propagates cast constraint.
// Rule 1:
// Input: bool2int(b, c) or bool2int(t, x)
// Output: int_eq(...)
//
// Rule 2:
// Input: bool2int(b, x)
// Action: Replace all instances of x by b.
// Output: inactive constraint
void Presolver::PresolveBool2Int(Constraint* ct) {
  DCHECK_EQ(ct->type, "bool2int");
  if (ct->arguments[0].HasOneValue() || ct->arguments[1].HasOneValue()) {
    // Rule 1.
    UpdateRuleStats("bool2int: rename to int_eq");
    ct->type = "int_eq";
  } else {
    // Rule 2.
    UpdateRuleStats("bool2int: merge boolean and integer variables.");
    AddVariableSubstitution(ct->arguments[1].Var(), ct->arguments[0].Var());
    ct->MarkAsInactive();
  }
}

// Propagates cast constraint.
// Rule 1:
// Input: int2float(x, y)
// Action: Replace all instances of y by x.
// Output: inactive constraint
void Presolver::PresolveInt2Float(Constraint* ct) {
  DCHECK_EQ(ct->type, "int2float");
  // Rule 1.
  UpdateRuleStats("int2float: merge integer and floating point variables.");
  AddVariableSubstitution(ct->arguments[1].Var(), ct->arguments[0].Var());
  ct->MarkAsInactive();
}

void Presolver::PresolveStoreFlatteningMapping(Constraint* ct) {
  CHECK_EQ(3, ct->arguments[1].variables.size());
  Variable* const var0 = ct->arguments[1].variables[0];
  Variable* const var1 = ct->arguments[1].variables[1];
  Variable* const var2 = ct->arguments[1].variables[2];
  const int64_t coeff0 = ct->arguments[0].values[0];
  const int64_t coeff1 = ct->arguments[0].values[1];
  const int64_t coeff2 = ct->arguments[0].values[2];
  const int64_t rhs = ct->arguments[2].Value();
  if (coeff0 == -1 && coeff2 == 1 && !array2d_index_map_.contains(var0)) {
    array2d_index_map_[var0] =
        Array2DIndexMapping(var1, coeff1, var2, -rhs, ct);
    UpdateRuleStats("int_lin_eq: store 2d flattening mapping");
  } else if (coeff0 == -1 && coeff1 == 1 &&
             !array2d_index_map_.contains(var0)) {
    array2d_index_map_[var0] =
        Array2DIndexMapping(var2, coeff2, var1, -rhs, ct);
    UpdateRuleStats("int_lin_eq: store 2d flattening mapping");
  } else if (coeff2 == -1 && coeff1 == 1 &&
             !array2d_index_map_.contains(var2)) {
    array2d_index_map_[var2] =
        Array2DIndexMapping(var0, coeff0, var1, -rhs, ct);
    UpdateRuleStats("int_lin_eq: store 2d flattening mapping");
  } else if (coeff2 == -1 && coeff0 == 1 &&
             !array2d_index_map_.contains(var2)) {
    array2d_index_map_[var2] =
        Array2DIndexMapping(var1, coeff1, var0, -rhs, ct);
    UpdateRuleStats("int_lin_eq: store 2d flattening mapping");
  }
}

namespace {
bool IsIncreasingAndContiguous(absl::Span<const int64_t> values) {
  for (int i = 0; i < values.size() - 1; ++i) {
    if (values[i + 1] != values[i] + 1) {
      return false;
    }
  }
  return true;
}

bool AreOnesFollowedByMinusOne(absl::Span<const int64_t> coeffs) {
  CHECK(!coeffs.empty());
  for (int i = 0; i < coeffs.size() - 1; ++i) {
    if (coeffs[i] != 1) {
      return false;
    }
  }
  return coeffs.back() == -1;
}

template <class T>
bool IsStrictPrefix(const std::vector<T>& v1, const std::vector<T>& v2) {
  if (v1.size() >= v2.size()) {
    return false;
  }
  for (int i = 0; i < v1.size(); ++i) {
    if (v1[i] != v2[i]) {
      return false;
    }
  }
  return true;
}
}  // namespace

// Rewrite array element: array_int_element:
//
// Rule 1:
// Input : array_int_element(x, [c1, .., cn], y) with x = a * x1 + x2 + b
// Output: array_int_element([x1, x2], [c_a1, .., c_am], b, [a, b])
//         to be interpreted by the extraction process.
//
// Rule 2:
// Input : array_int_element(x, [c1, .., cn], y) with x0 ci = c0 + i
// Output: int_lin_eq([-1, 1], [y, x], 1 - c)  (e.g. y = x + c - 1)
void Presolver::PresolveSimplifyElement(Constraint* ct) {
  if (ct->arguments[0].variables.size() != 1) return;
  Variable* const index_var = ct->arguments[0].Var();

  // Rule 1.
  if (array2d_index_map_.contains(index_var)) {
    UpdateRuleStats("array_int_element: rewrite as a 2d element");
    const Array2DIndexMapping& mapping = array2d_index_map_[index_var];
    // Rewrite constraint.
    ct->arguments[0] =
        Argument::VarRefArray({mapping.variable1, mapping.variable2});
    std::vector<int64_t> coefs;
    coefs.push_back(mapping.coefficient);
    coefs.push_back(1);
    ct->arguments.push_back(Argument::IntegerList(coefs));
    ct->arguments.push_back(Argument::IntegerValue(mapping.offset));
    index_var->active = false;
    mapping.constraint->MarkAsInactive();
    return;
  }

  // Rule 2.
  if (IsIncreasingAndContiguous(ct->arguments[1].values) &&
      ct->arguments[2].type == Argument::VAR_REF) {
    const int64_t start = ct->arguments[1].values.front();
    Variable* const index = ct->arguments[0].Var();
    Variable* const target = ct->arguments[2].Var();
    UpdateRuleStats("array_int_element: rewrite as a linear constraint");

    if (start == 1) {
      ct->type = "int_eq";
      ct->RemoveArg(1);
    } else {
      // Rewrite constraint into a int_lin_eq
      ct->type = "int_lin_eq";
      ct->arguments[0] = Argument::IntegerList({-1, 1});
      ct->arguments[1] = Argument::VarRefArray({target, index});
      ct->arguments[2] = Argument::IntegerValue(1 - start);
    }
  }
}

void Presolver::Run(Model* model) {
  // Should rewrite float constraints.
  if (absl::GetFlag(FLAGS_fz_floats_are_ints)) {
    // Treat float variables as int variables, convert constraints to int.
    for (Constraint* const ct : model->constraints()) {
      const std::string& id = ct->type;
      if (id == "int2float") {
        ct->type = "int_eq";
      } else if (id == "float_lin_le") {
        ct->type = "int_lin_le";
      } else if (id == "float_lin_eq") {
        ct->type = "int_lin_eq";
      }
    }
  }

  // Regroup increasing sequence of int_lin_eq([1,..,1,-1], [x1, ..., xn, yn])
  // into sequence of int_plus(x1, x2, y2), int_plus(y2, x3, y3)...
  std::vector<Variable*> current_variables;
  Variable* target_variable = nullptr;
  Constraint* first_constraint = nullptr;
  for (Constraint* const ct : model->constraints()) {
    if (target_variable == nullptr) {
      if (ct->type == "int_lin_eq" && ct->arguments[0].values.size() == 3 &&
          AreOnesFollowedByMinusOne(ct->arguments[0].values) &&
          ct->arguments[1].values.empty() && ct->arguments[2].Value() == 0) {
        current_variables = ct->arguments[1].variables;
        target_variable = current_variables.back();
        current_variables.pop_back();
        first_constraint = ct;
      }
    } else {
      if (ct->type == "int_lin_eq" &&
          AreOnesFollowedByMinusOne(ct->arguments[0].values) &&
          ct->arguments[0].values.size() == current_variables.size() + 2 &&
          IsStrictPrefix(current_variables, ct->arguments[1].variables)) {
        current_variables = ct->arguments[1].variables;
        // Rewrite ct into int_plus.
        ct->type = "int_plus";
        ct->arguments.clear();
        ct->arguments.push_back(Argument::VarRef(target_variable));
        ct->arguments.push_back(
            Argument::VarRef(current_variables[current_variables.size() - 2]));
        ct->arguments.push_back(Argument::VarRef(current_variables.back()));
        target_variable = current_variables.back();
        current_variables.pop_back();

        // We clean the first constraint too.
        if (first_constraint != nullptr) {
          first_constraint = nullptr;
        }
      } else {
        current_variables.clear();
        target_variable = nullptr;
      }
    }
  }

  // First pass.
  for (Constraint* const ct : model->constraints()) {
    if (ct->active && ct->type == "bool2int") {
      PresolveBool2Int(ct);
    } else if (ct->active && ct->type == "int2float") {
      PresolveInt2Float(ct);
    } else if (ct->active && ct->type == "int_lin_eq" &&
               ct->arguments[1].variables.size() == 3 &&
               ct->strong_propagation) {
      PresolveStoreFlatteningMapping(ct);
    }
  }
  if (!var_representative_map_.empty()) {
    // Some new substitutions were introduced. Let's process them.
    SubstituteEverywhere(model);
    var_representative_map_.clear();
    var_representative_vector_.clear();
  }

  // Second pass.
  for (Constraint* const ct : model->constraints()) {
    if (ct->type == "array_int_element" || ct->type == "array_bool_element") {
      PresolveSimplifyElement(ct);
    }
  }

  // Third pass: process objective with floating point coefficients.
  Variable* float_objective_var = nullptr;
  for (Variable* var : model->variables()) {
    if (!var->active) continue;
    if (var->domain.is_float) {
      CHECK(float_objective_var == nullptr);
      float_objective_var = var;
    }
  }

  Constraint* float_objective_ct = nullptr;
  if (float_objective_var != nullptr) {
    for (Constraint* ct : model->constraints()) {
      if (!ct->active) continue;
      if (ct->type == "float_lin_eq") {
        CHECK(float_objective_ct == nullptr);
        float_objective_ct = ct;
        break;
      }
    }
  }

  if (float_objective_ct != nullptr || float_objective_var != nullptr) {
    CHECK(float_objective_ct != nullptr);
    CHECK(float_objective_var != nullptr);
    const int arity = float_objective_ct->arguments[0].Size();
    CHECK_EQ(float_objective_ct->arguments[1].variables[arity - 1],
             float_objective_var);
    CHECK_EQ(float_objective_ct->arguments[0].floats[arity - 1], -1.0);
    for (int i = 0; i + 1 < arity; ++i) {
      model->AddFloatingPointObjectiveTerm(
          float_objective_ct->arguments[1].variables[i],
          float_objective_ct->arguments[0].floats[i]);
    }
    model->SetFloatingPointObjectiveOffset(
        -float_objective_ct->arguments[2].floats[0]);
    model->ClearObjective();
    float_objective_var->active = false;
    float_objective_ct->active = false;
  }

  // Report presolve rules statistics.
  if (!successful_rules_.empty()) {
    for (const auto& rule : successful_rules_) {
      if (rule.second == 1) {
        SOLVER_LOG(logger_, "  - rule '", rule.first, "' was applied 1 time");
      } else {
        SOLVER_LOG(logger_, "  - rule '", rule.first, "' was applied ",
                   rule.second, " times");
      }
    }
  }
}

// ----- Substitution support -----

void Presolver::AddVariableSubstitution(Variable* from, Variable* to) {
  CHECK(from != nullptr);
  CHECK(to != nullptr);
  // Apply the substitutions, if any.
  from = FindRepresentativeOfVar(from);
  to = FindRepresentativeOfVar(to);
  if (to->temporary) {
    // Let's switch to keep a non temporary as representative.
    Variable* tmp = to;
    to = from;
    from = tmp;
  }
  if (from != to) {
    CHECK(to->Merge(from->name, from->domain, from->temporary));
    from->active = false;
    var_representative_map_[from] = to;
    var_representative_vector_.push_back(from);
  }
}

Variable* Presolver::FindRepresentativeOfVar(Variable* var) {
  if (var == nullptr) return nullptr;
  Variable* start_var = var;
  // First loop: find the top parent.
  for (;;) {
    const auto& it = var_representative_map_.find(var);
    Variable* parent = it == var_representative_map_.end() ? var : it->second;
    if (parent == var) break;
    var = parent;
  }
  // Second loop: attach all the path to the top parent.
  while (start_var != var) {
    Variable* const parent = var_representative_map_[start_var];
    var_representative_map_[start_var] = var;
    start_var = parent;
  }
  const auto& iter = var_representative_map_.find(var);
  return iter == var_representative_map_.end() ? var : iter->second;
}

void Presolver::SubstituteEverywhere(Model* model) {
  // Rewrite the constraints.
  for (Constraint* const ct : model->constraints()) {
    if (ct != nullptr && ct->active) {
      for (int i = 0; i < ct->arguments.size(); ++i) {
        Argument& argument = ct->arguments[i];
        switch (argument.type) {
          case Argument::VAR_REF:
          case Argument::VAR_REF_ARRAY: {
            for (int i = 0; i < argument.variables.size(); ++i) {
              Variable* const old_var = argument.variables[i];
              Variable* const new_var = FindRepresentativeOfVar(old_var);
              if (new_var != old_var) {
                argument.variables[i] = new_var;
              }
            }
            break;
          }
          default: {
          }
        }
      }
    }
  }
  // Rewrite the search.
  for (Annotation* const ann : model->mutable_search_annotations()) {
    SubstituteAnnotation(ann);
  }
  // Rewrite the output.
  for (SolutionOutputSpecs* const output : model->mutable_output()) {
    output->variable = FindRepresentativeOfVar(output->variable);
    for (int i = 0; i < output->flat_variables.size(); ++i) {
      output->flat_variables[i] =
          FindRepresentativeOfVar(output->flat_variables[i]);
    }
  }
  // Do not forget to merge domain that could have evolved asynchronously
  // during presolve.
  for (const auto& iter : var_representative_map_) {
    iter.second->domain.IntersectWithDomain(iter.first->domain);
  }

  // Change the objective variable.
  Variable* const current_objective = model->objective();
  if (current_objective == nullptr) return;
  Variable* const new_objective = FindRepresentativeOfVar(current_objective);
  if (new_objective != current_objective) {
    model->SetObjective(new_objective);
  }
}

void Presolver::SubstituteAnnotation(Annotation* ann) {
  // TODO(user): Remove recursion.
  switch (ann->type) {
    case Annotation::ANNOTATION_LIST:
    case Annotation::FUNCTION_CALL: {
      for (int i = 0; i < ann->annotations.size(); ++i) {
        SubstituteAnnotation(&ann->annotations[i]);
      }
      break;
    }
    case Annotation::VAR_REF:
    case Annotation::VAR_REF_ARRAY: {
      for (int i = 0; i < ann->variables.size(); ++i) {
        ann->variables[i] = FindRepresentativeOfVar(ann->variables[i]);
      }
      break;
    }
    default: {
    }
  }
}

}  // namespace fz
}  // namespace operations_research
