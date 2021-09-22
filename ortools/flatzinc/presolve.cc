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

#include "ortools/flatzinc/presolve.h"

#include <cstdint>
#include <map>
#include <set>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "ortools/base/map_util.h"
#include "ortools/flatzinc/model.h"
#include "ortools/graph/cliques.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/vector_map.h"

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

// Minizinc flattens 2d element constraints (x = A[y][z]) into 1d element
// constraint with an affine mapping between y, z and the new index.
// This rule stores the mapping to reconstruct the 2d element constraint.
// This mapping can involve 1 or 2 variables depending if y or z in A[y][z]
// is a constant in the model).
void Presolver::PresolveStoreAffineMapping(Constraint* ct) {
  CHECK_EQ(2, ct->arguments[1].variables.size());
  Variable* const var0 = ct->arguments[1].variables[0];
  Variable* const var1 = ct->arguments[1].variables[1];
  const int64_t coeff0 = ct->arguments[0].values[0];
  const int64_t coeff1 = ct->arguments[0].values[1];
  const int64_t rhs = ct->arguments[2].Value();
  if (coeff0 == -1 && !affine_map_.contains(var0)) {
    affine_map_[var0] = AffineMapping(var1, coeff0, -rhs, ct);
    UpdateRuleStats("int_lin_eq: store affine mapping");
  } else if (coeff1 == -1 && !affine_map_.contains(var1)) {
    affine_map_[var1] = AffineMapping(var0, coeff0, -rhs, ct);
    UpdateRuleStats("int_lin_eq: store affine mapping");
  }
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
bool IsIncreasingAndContiguous(const std::vector<int64_t>& values) {
  for (int i = 0; i < values.size() - 1; ++i) {
    if (values[i + 1] != values[i] + 1) {
      return false;
    }
  }
  return true;
}

bool AreOnesFollowedByMinusOne(const std::vector<int64_t>& coeffs) {
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
// Input : array_int_element(x0, [c1, .., cn], y) with x0 = a * x + b
// Output: array_int_element(x, [c_a1, .., c_am], b) with a * i = b = ai
//
// Rule 2:
// Input : array_int_element(x, [c1, .., cn], y) with x = a * x1 + x2 + b
// Output: array_int_element([x1, x2], [c_a1, .., c_am], b, [a, b])
//         to be interpreted by the extraction process.
//
// Rule 3:
// Input: array_int_element(x, [c1, .., cn], y)
// Output array_int_element(x, [c1, .., c{max(x)}], y)
//
// Rule 4:
// Input : array_int_element(x, [c1, .., cn], y) with x0 ci = c0 + i
// Output: int_lin_eq([-1, 1], [y, x], 1 - c)  (e.g. y = x + c - 1)
void Presolver::PresolveSimplifyElement(Constraint* ct) {
  if (ct->arguments[0].variables.size() != 1) return;
  Variable* const index_var = ct->arguments[0].Var();

  // Rule 1.
  if (affine_map_.contains(index_var)) {
    const AffineMapping& mapping = affine_map_[index_var];
    const Domain& domain = mapping.variable->domain;
    if (domain.is_interval && domain.values.empty()) {
      // Invalid case. Ignore it.
      return;
    }
    if (domain.values[0] == 0 && mapping.coefficient == 1 &&
        mapping.offset > 1 && index_var->domain.is_interval) {
      // Simple translation
      const int offset = mapping.offset - 1;
      const int size = ct->arguments[1].values.size();
      for (int i = 0; i < size - offset; ++i) {
        ct->arguments[1].values[i] = ct->arguments[1].values[i + offset];
      }
      ct->arguments[1].values.resize(size - offset);
      affine_map_[index_var].constraint->arguments[2].values[0] = -1;
      affine_map_[index_var].offset = 1;
      index_var->domain.values[0] -= offset;
      index_var->domain.values[1] -= offset;
      UpdateRuleStats("array_int_element: simplify using affine mapping.");
      return;
    } else if (mapping.offset + mapping.coefficient > 0 &&
               domain.values[0] > 0) {
      const std::vector<int64_t>& values = ct->arguments[1].values;
      std::vector<int64_t> new_values;
      for (int64_t i = 1; i <= domain.values.back(); ++i) {
        const int64_t index = i * mapping.coefficient + mapping.offset - 1;
        if (index < 0) {
          return;
        }
        if (index > values.size()) {
          break;
        }
        new_values.push_back(values[index]);
      }
      // Rewrite constraint.
      UpdateRuleStats("array_int_element: simplify using affine mapping.");
      ct->arguments[0].variables[0] = mapping.variable;
      ct->arguments[0].variables[0]->domain.IntersectWithInterval(
          1, new_values.size());
      // TODO(user): Encapsulate argument setters.
      ct->arguments[1].values.swap(new_values);
      if (ct->arguments[1].values.size() == 1) {
        ct->arguments[1].type = Argument::INT_VALUE;
      }
      // Reset propagate flag.
      ct->presolve_propagation_done = false;
      // Mark old index var and affine constraint as presolved out.
      mapping.constraint->MarkAsInactive();
      index_var->active = false;
      return;
    }
  }

  // Rule 2.
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

  // Rule 3.
  if (index_var->domain.Max() < ct->arguments[1].values.size()) {
    // Reduce array of values.
    ct->arguments[1].values.resize(index_var->domain.Max());
    ct->presolve_propagation_done = false;
    UpdateRuleStats("array_int_element: reduce array");
    return;
  }

  // Rule 4.
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

// Simplifies array_var_int_element
//
// Input : array_var_int_element(x0, [x1, .., xn], y) with x0 = a * x + b
// Output: array_var_int_element(x, [x_a1, .., x_an], b) with a * i = b = ai
void Presolver::PresolveSimplifyExprElement(Constraint* ct) {
  if (ct->arguments[0].variables.size() != 1) return;

  Variable* const index_var = ct->arguments[0].Var();
  if (affine_map_.contains(index_var)) {
    const AffineMapping& mapping = affine_map_[index_var];
    const Domain& domain = mapping.variable->domain;
    if ((domain.is_interval && domain.values.empty()) ||
        domain.values[0] != 1 || mapping.offset + mapping.coefficient <= 0) {
      // Invalid case. Ignore it.
      return;
    }
    const std::vector<Variable*>& vars = ct->arguments[1].variables;
    std::vector<Variable*> new_vars;
    for (int64_t i = domain.values.front(); i <= domain.values.back(); ++i) {
      const int64_t index = i * mapping.coefficient + mapping.offset - 1;
      if (index < 0) {
        return;
      }
      if (index >= vars.size()) {
        break;
      }
      new_vars.push_back(vars[index]);
    }
    // Rewrite constraint.
    UpdateRuleStats("array_var_int_element: simplify using affine mapping.");
    ct->arguments[0].variables[0] = mapping.variable;
    // TODO(user): Encapsulate argument setters.
    ct->arguments[1].variables.swap(new_vars);
    // Mark old index var and affine constraint as presolved out.
    mapping.constraint->MarkAsInactive();
    index_var->active = false;
  } else if (index_var->domain.is_interval &&
             index_var->domain.values.size() == 2 &&
             index_var->domain.Max() < ct->arguments[1].variables.size()) {
    // Reduce array of variables.
    ct->arguments[1].variables.resize(index_var->domain.Max());
    UpdateRuleStats("array_var_int_element: reduce array");
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
    } else if (ct->active && ct->type == "int_lin_eq" &&
               ct->arguments[1].variables.size() == 2 &&
               ct->strong_propagation) {
      PresolveStoreAffineMapping(ct);
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
    if (ct->type == "array_var_int_element" ||
        ct->type == "array_var_bool_element") {
      PresolveSimplifyExprElement(ct);
    }
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
    Variable* parent = gtl::FindWithDefault(var_representative_map_, var, var);
    if (parent == var) break;
    var = parent;
  }
  // Second loop: attach all the path to the top parent.
  while (start_var != var) {
    Variable* const parent = var_representative_map_[start_var];
    var_representative_map_[start_var] = var;
    start_var = parent;
  }
  return gtl::FindWithDefault(var_representative_map_, var, var);
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
