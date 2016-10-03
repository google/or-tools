// Copyright 2010-2014 Google
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

#include "flatzinc/sat_fz_solver.h"

#include "base/map_util.h"
#include "flatzinc/logging.h"
#include "sat/disjunctive.h"
#include "sat/integer.h"
#include "sat/integer_expr.h"
#include "sat/intervals.h"
#include "sat/model.h"
#include "sat/optimization.h"
#include "sat/sat_solver.h"
#include "sat/table.h"

namespace operations_research {
namespace sat {

// Hold the sat::Model and the correspondance between flatzinc and sat vars.
struct SatModel {
  Model model;
  hash_map<fz::IntegerVariable*, IntegerVariable> var_map;

  // Utility function to conver an fz::Argument to sat::IntegerVariable(s).
  IntegerVariable LookupVar(const fz::Argument& argument) const;
  std::vector<IntegerVariable> LookupVars(const fz::Argument& argument) const;
};

// TODO(user): deal with constant variables.
// In a first version we could create constant IntegerVariable in the solver.
IntegerVariable SatModel::LookupVar(const fz::Argument& argument) const {
  CHECK_EQ(argument.type, fz::Argument::INT_VAR_REF);
  return FindOrDie(var_map, argument.variables[0]);
}

// TODO(user): deal with constant variables.
// In a first version we could create constant IntegerVariable in the solver.
std::vector<IntegerVariable> SatModel::LookupVars(
    const fz::Argument& argument) const {
  CHECK_EQ(argument.type, fz::Argument::INT_VAR_REF_ARRAY);
  std::vector<IntegerVariable> result;
  for (fz::IntegerVariable* var : argument.variables) {
    result.push_back(FindOrDie(var_map, var));
  }
  return result;
}

// =============================================================================
// Constraints extraction.
// =============================================================================

void ExtractIntMin(const fz::Constraint& ct, SatModel* m) {
  const IntegerVariable a = m->LookupVar(ct.arguments[0]);
  const IntegerVariable b = m->LookupVar(ct.arguments[1]);
  const IntegerVariable c = m->LookupVar(ct.arguments[2]);
  m->model.Add(IsEqualToMinOf(c, {a, b}));
}

// TODO(user): Optimize when num_vars is 1, 2, or vars contains const...
void ExtractArrayIntMinimum(const fz::Constraint& ct, SatModel* m) {
  const IntegerVariable min = m->LookupVar(ct.arguments[0]);
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
  m->model.Add(IsEqualToMinOf(min, vars));
}

void ExtractIntAbs(const fz::Constraint& ct, SatModel* m) {
  const IntegerVariable v = m->LookupVar(ct.arguments[0]);
  const IntegerVariable abs = m->LookupVar(ct.arguments[1]);
  m->model.Add(IsEqualToMaxOf(abs, {v, NegationOf(v)}));
}

// TODO(user): simplify when some variable are fixed!
void ExtractIntLinEq(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
  const std::vector<int64>& coeffs = ct.arguments[0].values;
  const int rhs = ct.arguments[2].values[0];
  m->model.Add(FixedWeightedSum(vars, coeffs, rhs));
}

// TODO(user): simplify when some variable are fixed!
void ExtractIntLinLe(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
  const std::vector<int64>& coeffs = ct.arguments[0].values;
  const int rhs = ct.arguments[2].values[0];
  m->model.Add(WeightedSumLowerOrEqual(vars, coeffs, rhs));
}

void ExtractRegular(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[0]);
  const int64 num_states = ct.arguments[1].Value();
  const int64 num_values = ct.arguments[2].Value();

  const std::vector<int64>& next = ct.arguments[3].values;
  std::vector<std::vector<int64>> transitions;
  int count = 0;
  for (int i = 1; i <= num_states; ++i) {
    for (int j = 1; j <= num_values; ++j) {
      transitions.push_back({i, j, next[count++]});
    }
  }

  const int64 initial_state = ct.arguments[4].Value();

  std::vector<int64> final_states;
  switch (ct.arguments[5].type) {
    case fz::Argument::INT_VALUE: {
      final_states.push_back(ct.arguments[5].values[0]);
      break;
    }
    case fz::Argument::INT_INTERVAL: {
      for (int v = ct.arguments[5].values[0]; v <= ct.arguments[5].values[1];
           ++v) {
        final_states.push_back(v);
      }
      break;
    }
    case fz::Argument::INT_LIST: {
      final_states = ct.arguments[5].values;
      break;
    }
    default: { LOG(FATAL) << "Wrong constraint " << ct.DebugString(); }
  }

  m->model.Add(
      TransitionConstraint(vars, transitions, initial_state, final_states));
}

// Returns false iff the constraint type is not supported.
bool ExtractConstraint(const fz::Constraint& ct, SatModel* m) {
  if (ct.type == "int_min") {
    ExtractIntMin(ct, m);
  } else if (ct.type == "array_int_minimum") {
    ExtractArrayIntMinimum(ct, m);
  } else if (ct.type == "int_abs") {
    ExtractIntAbs(ct, m);
  } else if (ct.type == "int_lin_eq") {
    ExtractIntLinEq(ct, m);
  } else if (ct.type == "int_lin_le") {
    ExtractIntLinLe(ct, m);
  } else if (ct.type == "regular") {
    ExtractRegular(ct, m);
  } else {
    return false;
  }
  return true;
}

// =============================================================================
// SAT/CP flatzinc solver.
// =============================================================================

// The format is fixed in the flatzinc specification.
std::string SolutionString(
    const Model& sat_model,
    const hash_map<fz::IntegerVariable*, IntegerVariable>& var_map,
    const fz::SolutionOutputSpecs& output) {
  if (output.variable != nullptr) {
    const int64 value =
        sat_model.Get(Value(FindOrDie(var_map, output.variable)));
    if (output.display_as_boolean) {
      return StringPrintf("%s = %s;", output.name.c_str(),
                          value == 1 ? "true" : "false");
    } else {
      return StringPrintf("%s = %" GG_LL_FORMAT "d;", output.name.c_str(),
                          value);
    }
  } else {
    const int bound_size = output.bounds.size();
    std::string result =
        StringPrintf("%s = array%dd(", output.name.c_str(), bound_size);
    for (int i = 0; i < bound_size; ++i) {
      if (output.bounds[i].max_value != 0) {
        result.append(StringPrintf("%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d, ",
                                   output.bounds[i].min_value,
                                   output.bounds[i].max_value));
      } else {
        result.append("{},");
      }
    }
    result.append("[");
    for (int i = 0; i < output.flat_variables.size(); ++i) {
      const int64 value =
          sat_model.Get(Value(FindOrDie(var_map, output.flat_variables[i])));
      if (output.display_as_boolean) {
        result.append(StringPrintf(value ? "true" : "false"));
      } else {
        result.append(StringPrintf("%" GG_LL_FORMAT "d", value));
      }
      if (i != output.flat_variables.size() - 1) {
        result.append(", ");
      }
    }
    result.append("]);");
    return result;
  }
  return "";
}

void SolveWithSat(const fz::Model& fz_model, const fz::FlatzincParameters& p) {
  SatModel m;

  // Extract all the variables.
  FZLOG << "Extracting " << fz_model.variables().size() << " variables. "
        << FZENDL;
  for (fz::IntegerVariable* var : fz_model.variables()) {
    if (!var->active) continue;
    m.var_map[var] =
        m.model.Add(NewIntegerVariable(var->domain.Min(), var->domain.Max()));

    // We fully encode a variable if it is given as a list of values. Otherwise,
    // we will lazily-encode it depending on the constraints it appears in or if
    // needed to fully specify a solution.
    if (!var->domain.is_interval) {
      m.model.GetOrCreate<IntegerEncoder>()->FullyEncodeVariable(
          FindOrDie(m.var_map, var),
          std::vector<IntegerValue>(var->domain.values.begin(),
                               var->domain.values.end()));
    }
  }

  // Extract all the constraints.
  FZLOG << "Extracting " << fz_model.constraints().size() << " constraints. "
        << FZENDL;
  std::set<std::string> unsupported_types;
  for (fz::Constraint* ct : fz_model.constraints()) {
    if (ct != nullptr && ct->active) {
      if (!ExtractConstraint(*ct, &m)) {
        unsupported_types.insert(ct->type);
      }
    }
  }
  if (!unsupported_types.empty()) {
    FZLOG << "There is unsuported constraints types in this model: " << FZENDL;
    for (const std::string& type : unsupported_types) {
      FZLOG << " - " << type;
    }
    return;
  }

  std::string solution_string;
  std::string search_status;

  // TODO(user): deal with other search parameters.
  FZLOG << "Solving..." << FZENDL;
  if (fz_model.objective() == nullptr) {
    // Decision problem.
    CHECK_EQ(1, p.num_solutions);

    // TODO(user): Encode IntegerVariable that are not fixed at the end of the
    // search.
    const SatSolver::Status status = m.model.GetOrCreate<SatSolver>()->Solve();
    FZLOG << "Status: " << status << FZENDL;

    if (status == SatSolver::MODEL_SAT) {
      for (const fz::SolutionOutputSpecs& output : fz_model.output()) {
        solution_string.append(SolutionString(m.model, m.var_map, output));
        solution_string.append("\n");
      }
    } else {
      search_status = "=====UNSATISFIABLE=====";
    }
  } else {
    // Optimization problem.
    int solution_number = 0;
    const IntegerVariable objective_var =
        FindOrDie(m.var_map, fz_model.objective());
    std::vector<IntegerVariable> decision_vars;
    for (const auto& entry : m.var_map) decision_vars.push_back(entry.second);
    MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
        /*log_info=*/false, objective_var, decision_vars,
        [objective_var, &solution_number, &solution_string, &m,
         &fz_model](const Model& sat_model) {
          // Overwrite solution_string with the new (better) solution.
          FZLOG << "Solution #" << ++solution_number
                << " obj:" << sat_model.Get(LowerBound(objective_var))
                << " num_bool:" << sat_model.Get<SatSolver>()->NumVariables()
                << FZENDL;
          solution_string.clear();
          for (const fz::SolutionOutputSpecs& output : fz_model.output()) {
            solution_string.append(
                SolutionString(sat_model, m.var_map, output));
            solution_string.append("\n");
          }
        },
        &m.model);
    if (solution_number > 0) {
      search_status = "==========";
    } else {
      search_status = "=====UNSATISFIABLE=====";
    }
  }

  // Print the solution.
  std::cout << solution_string << "----------" << std::endl;
  if (!search_status.empty()) {
    std::cout << search_status << std::endl;
  }
}

}  // namespace sat
}  // namespace operations_research
