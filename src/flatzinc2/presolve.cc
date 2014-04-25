// Copyright 2010-2013 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in comÏpliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS% IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "base/map_util.h"
#include "base/strutil.h"
#include "flatzinc2/presolve.h"

DECLARE_bool(logging);
DECLARE_bool(verbose_logging);

namespace operations_research {

// For the author's reference, here is an indicative list of presolve rules that
// should eventually be implemented.
//
// Presolve rule:
//   - int_lin_le/eq -> all >0 -> bound propagation on max
//   - array_xxx_element and index is affine -> reduce array
//   - chain of int_min, int_max -> regroup into maximum_int, minimum_int

// ----- Presolve rules -----

bool FzPresolver::PresolveBool2Int(FzConstraint* ct) {
  MarkVariablesAsEquivalent(ct->GetVar(0), ct->GetVar(1));
  ct->MarkAsTriviallyTrue();
  return true;
}

bool FzPresolver::PresolveIntEq(FzConstraint* ct) {
  if (ct->IsIntegerVariable(0)) {
    if (ct->IsIntegerVariable(1)) {
      MarkVariablesAsEquivalent(ct->GetVar(0), ct->GetVar(1));
      ct->MarkAsTriviallyTrue();
      return true;
    } else if (ct->IsBound(1)) {
      const int64 value = ct->GetBound(1);
      ct->GetVar(0)->domain.ReduceDomain(value, value);
      ct->MarkAsTriviallyTrue();
      return true;
    }
  } else if (ct->IsBound(0)) {  // Arg0 is an integer value.
    const int64 value = ct->GetBound(0);
    if (ct->IsIntegerVariable(1)) {
      ct->GetVar(1)->domain.ReduceDomain(value, value);
      ct->MarkAsTriviallyTrue();
      return true;
    } else if (ct->IsBound(1) && value == ct->GetBound(1)) {
      // No-op, removing.
      ct->MarkAsTriviallyTrue();
      return false;
    }
  }
  return false;
}

bool FzPresolver::PresolveIntNe(FzConstraint* ct) {
  if ((ct->IsIntegerVariable(0) && ct->IsBound(1) &&
       ct->GetVar(0)->domain.RemoveValue(ct->GetBound(1))) ||
      (ct->IsIntegerVariable(1) && ct->IsBound(0) &&
       ct->GetVar(1)->domain.RemoveValue(ct->GetBound(0)))) {
    FZVLOG << "Propagate " << ct->DebugString() << std::endl;
    return true;
  }
  return false;
}

bool FzPresolver::PresolveInequalities(FzConstraint* ct) {
  const std::string& id = ct->type;
  if (ct->IsIntegerVariable(0) && ct->IsBound(1)) {
    FzIntegerVariable* const var = ct->GetVar(0);
    const int64 value = ct->GetBound(1);
    if (id == "int_le") {
      var->domain.ReduceDomain(kint64min, value);
    } else if (id == "int_lt") {
      var->domain.ReduceDomain(kint64min, value - 1);
    } else if (id == "int_ge") {
      var->domain.ReduceDomain(value, kint64max);
    } else if (id == "int_gt") {
      var->domain.ReduceDomain(value + 1, kint64max);
    }
    ct->MarkAsTriviallyTrue();
    return true;
  } else if (ct->IsBound(0) && ct->IsIntegerVariable(1)) {
    FzIntegerVariable* const var = ct->GetVar(1);
    const int64 value = ct->GetBound(0);
    if (id == "int_le") {
      var->domain.ReduceDomain(value, kint64max);
    } else if (id == "int_lt") {
      var->domain.ReduceDomain(value + 1, kint64max);
    } else if (id == "int_ge") {
      var->domain.ReduceDomain(kint64min, value);
    } else if (id == "int_gt") {
      var->domain.ReduceDomain(kint64min, value - 1);
    }
    ct->MarkAsTriviallyTrue();
    return true;
  }
  return false;
}

void FzPresolver::Unreify(FzConstraint* ct) {
  const std::string& id = ct->type;
  const int last_argument = ct->arguments.size() - 1;
  ct->type.resize(id.size() - 5);
  FzIntegerVariable* const bool_var = ct->target_variable;
  if (bool_var != nullptr) {
    // DCHECK bool_var is bound
    ct->target_variable = bool_var;
    bool_var->defining_constraint = nullptr;
  }
  ct->arguments.pop_back();
  if (ct->GetBound(last_argument) == 1) {
    FZVLOG << "Unreify " << ct->DebugString() << std::endl;
  } else {
    FZVLOG << "Unreify and inverse " << ct->DebugString() << std::endl;
    if (id == "int_eq")
      ct->type = "int_ne";
    else if (id == "int_ne")
      ct->type = "int_eq";
    else if (id == "int_ge")
      ct->type = "int_lt";
    else if (id == "int_gt")
      ct->type = "int_le";
    else if (id == "int_le")
      ct->type = "int_gt";
    else if (id == "int_lt")
      ct->type = "int_ge";
    else if (id == "int_lin_eq")
      ct->type = "int_lin_ne";
    else if (id == "int_lin_ne")
      ct->type = "int_lin_eq";
    else if (id == "int_lin_ge")
      ct->type = "int_lin_lt";
    else if (id == "int_lin_gt")
      ct->type = "int_lin_le";
    else if (id == "int_lin_le")
      ct->type = "int_lin_gt";
    else if (id == "int_lin_lt")
      ct->type = "int_lin_ge";
    else if (id == "bool_eq")
      ct->type = "bool_ne";
    else if (id == "bool_ne")
      ct->type = "bool_eq";
    else if (id == "bool_ge")
      ct->type = "bool_lt";
    else if (id == "bool_gt")
      ct->type = "bool_le";
    else if (id == "bool_le")
      ct->type = "bool_gt";
    else if (id == "bool_lt")
      ct->type = "bool_ge";
  }
}

bool FzPresolver::PresolveSetIn(FzConstraint* ct) {
  if (ct->IsIntegerVariable(0) &&
      ct->arguments[1].type == FzArgument::INT_DOMAIN) {
    FZVLOG << "Propagate " << ct->DebugString() << std::endl;
    ct->GetVar(0)->domain.IntersectWith(ct->arguments[1].domain);
    ct->MarkAsTriviallyTrue();
    return true;
  }
  return false;
}

bool FzPresolver::PresolveArrayBoolOr(FzConstraint* ct) {
  if (ct->IsBound(1) && ct->GetBound(1) == 0) {
    for (const FzIntegerVariable* const var : ct->arguments[0].variables) {
      if (!var->domain.Contains(0)) {
        return false;
      }
    }
    FZVLOG << "Propagate " << ct->DebugString() << std::endl;
    for (FzIntegerVariable* const var : ct->arguments[0].variables) {
      var->domain.ReduceDomain(0, 0);
    }
    return true;
  }
  return false;
}

bool PresolveIntTimes(FzConstraint* ct) {
  if (ct->IsBound(0) && ct->IsBound(1) && ct->IsIntegerVariable(2) &&
      !ct->IsBound(2)) {
    FZVLOG << " Propagate " << ct->DebugString() << std::endl;
    const int64 value = ct->GetBound(0) * ct->GetBound(1);
    ct->GetVar(2)->domain.ReduceDomain(value, value);
    return true;
  }
  return false;
}

bool PresolveIntDiv(FzConstraint* ct) {
  if (ct->IsBound(0) && ct->IsBound(1) && ct->IsIntegerVariable(2) &&
      !ct->IsBound(2)) {
    FZVLOG << " Propagate " << ct->DebugString() << std::endl;
    const int64 value = ct->GetBound(0) / ct->GetBound(1);
    ct->GetVar(2)->domain.ReduceDomain(value, value);
    return true;
  }
  return false;
}

bool FzPresolver::PresolveArrayBoolAnd(FzConstraint* ct) {
  if (ct->IsBound(1) && ct->GetBound(1) == 1) {
    for (const FzIntegerVariable* const var : ct->arguments[0].variables) {
      if (!var->domain.Contains(1)) {
        return false;
      }
    }
    FZVLOG << "Propagate " << ct->DebugString() << std::endl;
    for (FzIntegerVariable* const var : ct->arguments[0].variables) {
      var->domain.ReduceDomain(1, 1);
    }
    return true;
  }
  return false;
}

bool FzPresolver::PresolveBoolEqNeReif(FzConstraint* ct) {
  if (ct->IsBound(1)) {
    FZVLOG << "Simplify " << ct->DebugString() << std::endl;
    const int64 value = ct->GetBound(1);
    // Remove boolean value argument.
    ct->arguments[1] = ct->arguments[2];
    ct->arguments.pop_back();
    // Change type.
    ct->type = ((ct->type == "bool_eq_reif" && value == 1) ||
                (ct->type == "bool_ne_reif" && value == 0))
                   ? "bool_eq"
                   : "bool_not";
    return true;
  }
  return false;
}

bool FzPresolver::PresolveIntLinGt(FzConstraint* ct) {
  FZVLOG << "Transform " << ct->DebugString() << " into int_lin_ge"
         << std::endl;
  ct->arguments[2].integer_value++;
  ct->type = "int_lin_ge";
  return true;
}

bool FzPresolver::PresolveIntLinLt(FzConstraint* ct) {
  FZVLOG << "Transform " << ct->DebugString() << " into int_lin_le"
         << std::endl;
  ct->arguments[2].integer_value--;
  ct->type = "int_lin_le";
  return true;
}

bool FzPresolver::PresolveArrayIntElement(FzConstraint* ct) {
  if (ct->IsIntegerVariable(2)) {
    FZVLOG << "Propagate domain on " << ct->DebugString() << std::endl;
    ct->GetVar(2)->domain.IntersectWith(ct->arguments[1].domain);
    return true;
  }
  return false;
}

bool FzPresolver::PresolveLinear(FzConstraint* ct) {
  if (ct->arguments[0].domain.values.empty()) {
    return false;
  }
  for (const int64 coef : ct->arguments[0].domain.values) {
    if (coef > 0) {
      return false;
    }
  }
  FZVLOG << "Reverse " << ct->DebugString() << std::endl;
  for (int64& coef : ct->arguments[0].domain.values) {
    coef *= -1;
  }
  ct->arguments[2].integer_value *= -1;
  const std::string& id = ct->type;
  if (id == "int_lin_le") {
    ct->type = "int_lin_ge";
  } else if (id == "int_lin_lt") {
    ct->type = "int_lin_gt";
  } else if (id == "int_lin_le") {
    ct->type = "int_lin_ge";
  } else if (id == "int_lin_lt") {
    ct->type = "int_lin_gt";
  } else if (id == "int_lin_le_reif") {
    ct->type = "int_lin_ge_reif";
  } else if (id == "int_lin_ge_reif") {
    ct->type = "int_lin_le_reif";
  }
  return false;
}

bool FzPresolver::PresolveOneConstraint(FzConstraint* ct) {
  bool changed = false;
  const std::string& id = ct->type;
  const int num_arguments = ct->arguments.size();
  if (HasSuffixString(id, "_reif") && ct->IsBound(num_arguments - 1)) {
    Unreify(ct);
    changed = true;
  }
  if (id == "int_eq") {
    changed |= PresolveIntEq(ct);
  }
  if (id == "int_ne") {
    changed |= PresolveIntNe(ct);
  }
  if (id == "bool2int") {
    changed |= PresolveBool2Int(ct);
  }
  if (id == "int_le" || id == "int_lt" || id == "int_ge" || id == "int_gt") {
    changed |= PresolveInequalities(ct);
  }
  if (id == "int_abs" && !ContainsKey(abs_map_, ct->GetVar(1))) {
    // Stores abs() map.
    FZVLOG << "Stores abs map for " << ct->DebugString() << std::endl;
    abs_map_[ct->GetVar(1)] = ct->GetVar(0);
    changed = true;
  }
  if ((id == "int_eq_reif" || id == "int_ne_reif" || id == "int_ne") &&
      ct->IsBound(1) && ct->GetBound(1) == 0 &&
      ContainsKey(abs_map_, ct->GetVar(0))) {
    FZVLOG << "Remove abs() from " << ct->DebugString() << std::endl;
    ct->arguments[0].variable = abs_map_[ct->GetVar(0)];
    changed = true;
  }
  if (id == "set_in") {
    changed |= PresolveSetIn(ct);
  }
  if (id == "array_bool_and") {
    changed |= PresolveArrayBoolAnd(ct);
  }
  if (id == "array_bool_or") {
    changed |= PresolveArrayBoolOr(ct);
  }
  if (id == "bool_eq_reif" || id == "bool_ne_reif") {
    changed |= PresolveBoolEqNeReif(ct);
  }
  if (id == "array_int_element") {
    changed |= PresolveArrayIntElement(ct);
  }
  if (id == "int_div") {
    changed |= PresolveIntDiv(ct);
  }
  if (id == "int_times") {
    changed |= PresolveIntTimes(ct);
  }
  if (id == "int_lin_gt") {
    changed |= PresolveIntLinGt(ct);
  }
  if (id == "int_lin_lt") {
    changed |= PresolveIntLinLt(ct);
  }
  if (HasPrefixString(id, "int_lin_")) {
    changed |= PresolveLinear(ct);
  }
  return changed;
}

bool FzPresolver::Run(FzModel* model) {
  bool changed_since_start = false;
  for (;;) {
    bool changed = false;
    var_representative_map_.clear();
    for (FzConstraint* const ct : model->constraints()) {
      if (!ct->is_trivially_true) {
        changed |= PresolveOneConstraint(ct);
      }
    }
    if (!var_representative_map_.empty()) {
      // Some new substitutions were introduced. Let's process them.
      DCHECK(changed);
      changed = true;  // To be safe in opt mode.
      SubstituteEverywhere(model);
    }
    changed_since_start |= changed;
    if (!changed) break;
  }
  return changed_since_start;
}

// ----- Substitution support -----

void FzPresolver::MarkVariablesAsEquivalent(FzIntegerVariable* from,
                                            FzIntegerVariable* to) {
  CHECK(from != nullptr);
  CHECK(to != nullptr);
  // Apply the substitutions, if any.
  from = FindRepresentativeOfVar(from);
  to = FindRepresentativeOfVar(to);
  if (to->temporary) {
    // Let's switch to keep a non temporary as representative.
    FzIntegerVariable* tmp = to;
    to = from;
    from = tmp;
  }
  if (from != to) {
    FZVLOG << "Mark " << from->DebugString() << " as equivalent to "
           << to->DebugString() << std::endl;
    CHECK(to->Merge(from->name, from->domain, from->defining_constraint,
                    from->temporary));
    var_representative_map_[from] = to;
  }
}

FzIntegerVariable* FzPresolver::FindRepresentativeOfVar(
    FzIntegerVariable* var) {
  if (var == nullptr) return nullptr;
  FzIntegerVariable* start_var = var;
  // First loop: find the top parent.
  for (;;) {
    FzIntegerVariable* parent =
        FindWithDefault(var_representative_map_, var, var);
    if (parent == var) break;
    var = parent;
  }
  // Second loop: attach all the path to the top parent.
  while (start_var != var) {
    FzIntegerVariable* const parent = var_representative_map_[start_var];
    var_representative_map_[start_var] = var;
    start_var = parent;
  }
  return FindWithDefault(var_representative_map_, var, var);
}

void FzPresolver::SubstituteEverywhere(FzModel* model) {
  // Rewrite the constraints.
  for (FzConstraint* const ct : model->constraints()) {
    if (ct != nullptr && !ct->is_trivially_true) {
      for (int i = 0; i < ct->arguments.size(); ++i) {
        FzArgument* argument = &ct->arguments[i];
        switch (argument->type) {
          case FzArgument::INT_VAR_REF: {
            argument->variable = FindRepresentativeOfVar(argument->variable);
            break;
          }
          case FzArgument::INT_VAR_REF_ARRAY: {
            for (int i = 0; i < argument->variables.size(); ++i) {
              argument->variables[i] =
                  FindRepresentativeOfVar(argument->variables[i]);
            }
            break;
          }
          default: {}
        }
      }
      ct->target_variable = FindRepresentativeOfVar(ct->target_variable);
    }
  }
  // Rewrite the search.
  for (FzAnnotation* const ann : model->mutable_search_annotations()) {
    SubstituteAnnotation(ann);
  }
  // Rewrite the output.
  for (FzOnSolutionOutput* const output : model->mutable_output()) {
    output->variable = FindRepresentativeOfVar(output->variable);
    for (int i = 0; i < output->flat_variables.size(); ++i) {
      output->flat_variables[i] =
          FindRepresentativeOfVar(output->flat_variables[i]);
    }
  }
}

void FzPresolver::SubstituteAnnotation(FzAnnotation* ann) {
  // TODO(user): Remove recursion.
  switch (ann->type) {
    case FzAnnotation::ANNOTATION_LIST:
    case FzAnnotation::FUNCTION_CALL: {
      for (int i = 0; i < ann->annotations.size(); ++i) {
        SubstituteAnnotation(&ann->annotations[i]);
      }
      break;
    }
    case FzAnnotation::INT_VAR_REF: {
      ann->variable = FindRepresentativeOfVar(ann->variable);
      break;
    }
    case FzAnnotation::INT_VAR_REF_ARRAY: {
      for (int i = 0; i < ann->variables.size(); ++i) {
        ann->variables[i] = FindRepresentativeOfVar(ann->variables[i]);
      }
      break;
    }
    default: {}
  }
}

// ----- Clean up model -----

void FzPresolver::CleanUpModelForTheCpSolver(FzModel* model) {
  // First pass.
  for (FzConstraint* const ct : model->constraints()) {
    const std::string& id = ct->type;
    // Remove useless annotations on int_lin_eq.
    if (id == "int_lin_eq") {
      if (ct->arguments[0].domain.values.size() > 2 &&
          ct->target_variable != nullptr) {
        ct->RemoveTargetVariable();
        return;
      } else if (ct->arguments[0].domain.values.size() <= 2 &&
                 ct->strong_propagation) {
        FZVLOG << "Remove strong_propagation from " << ct->DebugString()
               << std::endl;
        ct->strong_propagation = false;
        return;
      }
    }
    // Remove target variables from constraints passed to SAT.
    if (ct->target_variable != nullptr &&
        (id == "array_bool_and" || id == "array_bool_or" ||
         id == "bool_eq_reif" || id == "bool_ne_reif" || id == "bool_le_reif" ||
         id == "bool_ge_reif")) {
      ct->RemoveTargetVariable();
    }
  }
  // Second pass.
  for (FzConstraint* const ct : model->constraints()) {
    const std::string& id = ct->type;
    // Create new target variables with unused boolean variables.
    if (ct->target_variable == nullptr &&
        (id == "int_lin_eq_reif" || id == "int_lin_ne_reif" ||
         id == "int_lin_ge_reif" || id == "int_lin_le_reif" ||
         id == "int_lin_gt_reif" || id == "int_lin_lt_reif" ||
         id == "int_eq_reif" || id == "int_ne_reif" || id == "int_le_reif" ||
         id == "int_ge_reif" || id == "int_lt_reif" || id == "int_gt_reif")) {
      FzIntegerVariable* const bool_var = ct->GetVar(2);
      if (bool_var != nullptr && bool_var->defining_constraint == nullptr) {
        FZVLOG << "Create target_variable on " << ct->DebugString()
               << std::endl;
        ct->target_variable = bool_var;
        bool_var->defining_constraint = ct;
      }
    }
  }
}
}  // namespace operations_research
