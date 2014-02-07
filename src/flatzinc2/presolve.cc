// Copyright 2010-2013 Google
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
#include "base/map-util.h"
#include "base/stl_util.h"
#include "flatzinc2/presolve.h"

namespace operations_research {

FzPresolve::~FzPresolve() {}

void FzPresolve::Init() {
  Register("bool2int", &FzPresolve::PresolveBool2Int);
  Register("int_eq", &FzPresolve::PresolveIntEq);
}

void FzPresolve::Register(const std::string& id, FzPresolveRule const rule) {
  rules_[id].push_back(rule);
}

FzPresolve::PresolveStatus FzPresolve::PresolveBool2Int(
    FzConstraint* const input, FzConstraint** output) {
  return AddSubstitution(input->arguments[0].variable,
                         input->arguments[1].variable)
             ? REMOVE_ME
             : NO_CHANGE;
}

FzPresolve::PresolveStatus FzPresolve::PresolveIntEq(FzConstraint* const input,
                                                     FzConstraint** output) {
  if (input->arguments[0].type == FzArgument::INT_VAR_REF) {
    if (input->arguments[1].type == FzArgument::INT_VAR_REF) {
      FzIntegerVariable* const left = input->arguments[0].variable;
      FzIntegerVariable* const right = input->arguments[1].variable;
      if ((left->temporary && AddSubstitution(left, right)) ||
          (right->temporary && AddSubstitution(right, left)) ||
          AddSubstitution(left, right) || AddSubstitution(right, left)) {
        return REMOVE_ME;
      } else {
        return NO_CHANGE;
      }
    } else {
      const int64 value = input->arguments[1].integer_value;
      input->arguments[0].variable->domain.ReduceDomain(value, value);
      return REMOVE_ME;
    }
  } else {
    const int64 value = input->arguments[0].integer_value;
    if (input->arguments[1].type == FzArgument::INT_VAR_REF) {
      input->arguments[1].variable->domain.ReduceDomain(value, value);
      return REMOVE_ME;
    } else {
      if (value == input->arguments[1].integer_value) {
        // No-op, removing.
        return REMOVE_ME;
      } else {
        // Rewrite into false constraint.
        return NO_CHANGE;
      }
    }
  }
}

bool FzPresolve::Run(FzModel* const model) {
  for (;;) {
    bool changed = false;
    var_substitution_map_.clear();
    for (int i = 0; i < model->constraints().size(); ++i) {
      FzConstraint* const ct = model->constraints()[i];
      if (ct != nullptr && ContainsKey(rules_, ct->type)) {
        const std::vector<FzPresolveRule>& all_rules = rules_[ct->type];
        bool still_valid = true;
        for (int i = 0; i < all_rules.size(); ++i) {
          FzPresolveRule const rule = all_rules[i];
          FzConstraint* output = nullptr;
          switch ((this->*rule)(ct, &output)) {
            case SOME_PRESOLVE: {
              changed = true;
              break;
            }
            case REMOVE_ME: {
              model->DeleteConstraintAtIndex(i);
              still_valid = false;
              changed = true;
              break;
            }
            case NO_CHANGE: { break; }
          }
        }
        if (!still_valid) {
          continue;
        }
      }
    }
    if (!var_substitution_map_.empty()) {
      // Some new substitutions were introduced. Let's process them.
      changed = true;  // Safe assumption.
      // Rewrite the constraints.
      for (int i = 0; i < model->constraints().size(); ++i) {
        if (model->constraints()[i] != nullptr) {
          SubstituteConstraint(model->constraints()[i]);
        }
      }
      // Rewrite the search.
      for (int i = 0; i < model->search_annotations().size(); ++i) {
        SubstituteAnnotation(model->mutable_search_annotations(i));
      }
      // Rewrite the output.
      for (int i = 0; i < model->output().size(); ++i) {
        SubstituteOutput(model->mutable_output(i));
      }
    }
    if (!changed) {
      break;
    }
  }
  return true;
}

FzPresolve::PresolveStatus FzPresolve::PresolveOneConstraint(
    FzConstraint* const ct, FzConstraint** output) {
  bool changed = false;
  if (ct != nullptr && ContainsKey(rules_, ct->type)) {
    const std::vector<FzPresolveRule>& all_rules = rules_[ct->type];
    for (int i = 0; i < all_rules.size(); ++i) {
      FzPresolveRule const rule = all_rules[i];
      switch ((this->*rule)(ct, output)) {
        case SOME_PRESOLVE: {
          changed = true;
          break;
        }
        case REMOVE_ME: { return REMOVE_ME; }
        case REWRITE_ME: { return REWRITE_ME; }
        case NO_CHANGE: { break; }
      }
    }
  }
  return changed ? SOME_PRESOLVE : NO_CHANGE;
}

bool FzPresolve::AddSubstitution(FzIntegerVariable* const from,
                                 FzIntegerVariable* const to) {
  FzIntegerVariable* destination = FindPtrOrNull(var_substitution_map_, to);
  if (destination == nullptr) {
    destination = to;
  }
  FzIntegerVariable* source = FindPtrOrNull(var_substitution_map_, from);
  if (source == nullptr) {
    source = from;
  }
  if (source == destination) {
    return false;
  }
  if (destination->Merge(source->name, source->domain,
                         source->defining_constraint, source->temporary)) {
    var_substitution_map_[source] = destination;
    return true;
  } else {
    return false;
  }
}

void FzPresolve::SubstituteArgument(FzArgument* const argument) {
  switch (argument->type) {
    case FzArgument::INT_VAR_REF: {
      argument->variable = FindWithDefault(
          var_substitution_map_, argument->variable, argument->variable);
      break;
    }
    case FzArgument::INT_VAR_REF_ARRAY: {
      for (int i = 0; i < argument->variables.size(); ++i) {
        argument->variables[i] =
            FindWithDefault(var_substitution_map_, argument->variables[i],
                            argument->variables[i]);
      }
      break;
    }
    default: {}
  }
}

void FzPresolve::SubstituteAnnotation(FzAnnotation* const ann) {
  switch (ann->type) {
    case FzAnnotation::ANNOTATION_LIST: {
      for (int i = 0; i < ann->annotations.size(); ++i) {
        SubstituteAnnotation(&ann->annotations[i]);
      }
      break;
    }
    case FzAnnotation::FUNCTION_CALL: {
      for (int i = 0; i < ann->annotations.size(); ++i) {
        SubstituteAnnotation(&ann->annotations[i]);
      }
      break;
    }
    case FzAnnotation::INT_VAR_REF: {
      FzIntegerVariable* const alt =
          FindPtrOrNull(var_substitution_map_, ann->variable);
      if (alt != nullptr) {
        ann->variable = alt;
      }
      break;
    }
    case FzAnnotation::INT_VAR_REF_ARRAY: {
      for (int i = 0; i < ann->variables.size(); ++i) {
        FzIntegerVariable* const alt =
            FindPtrOrNull(var_substitution_map_, ann->variables[i]);
        if (alt != nullptr) {
          ann->variables[i] = alt;
        }
      }
      break;
    }
    default: {}
  }
}

void FzPresolve::SubstituteConstraint(FzConstraint* const ct) {
  for (int i = 0; i < ct->arguments.size(); ++i) {
    SubstituteArgument(&ct->arguments[i]);
  }
  FzIntegerVariable* const alt =
      FindPtrOrNull(var_substitution_map_, ct->target_var);
  if (alt != nullptr) {
    ct->target_var = alt;
  }
}

void FzPresolve::SubstituteOutput(FzOnSolutionOutput* const output) {
  if (output->variable != nullptr) {
    FzIntegerVariable* const alt =
        FindPtrOrNull(var_substitution_map_, output->variable);
    if (alt != nullptr) {
      output->variable = alt;
    }
  }
  for (int i = 0; i < output->flat_variables.size(); ++i) {
    FzIntegerVariable* const alt =
        FindPtrOrNull(var_substitution_map_, output->flat_variables[i]);
    if (alt != nullptr) {
      output->flat_variables[i] = alt;
    }
  }
}
}  // namespace operations_research
