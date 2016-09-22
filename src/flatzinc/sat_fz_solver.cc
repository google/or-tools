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
#include "sat/sat_solver.h"
#include "sat/table.h"

namespace operations_research {
namespace sat {

// TODO(user): deal with constant variables.
// In a first version we could create constant IntegerVariable in the solver.
IntegerVariable LookupVar(
    const hash_map<fz::IntegerVariable*, IntegerVariable>& var_map,
    const fz::Argument& argument) {
  CHECK_EQ(argument.type, fz::Argument::INT_VAR_REF);
  return FindOrDie(var_map, argument.variables[0]);
}

// TODO(user): deal with constant variables.
// In a first version we could create constant IntegerVariable in the solver.
std::vector<IntegerVariable> LookupVars(
    const hash_map<fz::IntegerVariable*, IntegerVariable>& var_map,
    const fz::Argument& argument) {
  CHECK_EQ(argument.type, fz::Argument::INT_VAR_REF_ARRAY);
  std::vector<IntegerVariable> result;
  for (fz::IntegerVariable* var : argument.variables) {
    result.push_back(FindOrDie(var_map, var));
  }
  return result;
}

void ExtractIntMin(
    const fz::Constraint& ct,
    const hash_map<fz::IntegerVariable*, IntegerVariable>& var_map,
    Model* sat_model) {
  const IntegerVariable a = LookupVar(var_map, ct.arguments[0]);
  const IntegerVariable b = LookupVar(var_map, ct.arguments[1]);
  const IntegerVariable c = LookupVar(var_map, ct.arguments[2]);
  sat_model->Add(IsEqualToMinOf(c, {a, b}));
}

void ExtractIntAbs(
    const fz::Constraint& ct,
    const hash_map<fz::IntegerVariable*, IntegerVariable>& var_map,
    Model* sat_model) {
  const IntegerVariable v = LookupVar(var_map, ct.arguments[0]);
  const IntegerVariable abs = LookupVar(var_map, ct.arguments[1]);
  sat_model->Add(IsEqualToMaxOf(abs, {v, NegationOf(v)}));
}

void ExtractRegular(
    const fz::Constraint& ct,
    const hash_map<fz::IntegerVariable*, IntegerVariable>& var_map,
    Model* sat_model) {
  const std::vector<IntegerVariable> vars = LookupVars(var_map, ct.arguments[0]);
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

  sat_model->Add(
      TransitionConstraint(vars, transitions, initial_state, final_states));
}

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

void SolveWithSat(const fz::Model& model, const fz::FlatzincParameters& p) {
  Model sat_model;

  // Correspondance between a fz::IntegerVariable and a sat::IntegerVariable.
  hash_map<fz::IntegerVariable*, IntegerVariable> var_map;

  // Extract all the variables.
  FZLOG << "Extracting " << model.variables().size() << " variables. "
        << FZENDL;
  for (fz::IntegerVariable* var : model.variables()) {
    if (!var->active) continue;
    var_map[var] =
        sat_model.Add(NewIntegerVariable(var->domain.Min(), var->domain.Max()));

    // TODO(user): encode if it is a list of value? This way constraint using
    // it will get the intersection with the proper interval.
  }

  // Extract all the constraints.
  FZLOG << "Extracting " << model.constraints().size() << " constraints. "
        << FZENDL;
  std::set<std::string> unsupported_types;
  for (fz::Constraint* ct : model.constraints()) {
    if (ct != nullptr && ct->active) {
      if (ct->type == "int_min") {
        ExtractIntMin(*ct, var_map, &sat_model);
      } else if (ct->type == "int_abs") {
        ExtractIntAbs(*ct, var_map, &sat_model);
      } else if (ct->type == "regular") {
        ExtractRegular(*ct, var_map, &sat_model);
      } else {
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

  // For now assume a decision problem!
  //
  // TODO(user): deal with other kind of search (optim, all solutions, ...).
  //
  // TODO(user): Encode IntegerVariable that are not fixed at the end of the
  // search.
  CHECK(model.objective() == nullptr);
  CHECK_EQ(1, p.num_solutions);
  FZLOG << "Solving..." << FZENDL;
  const SatSolver::Status status = sat_model.GetOrCreate<SatSolver>()->Solve();
  FZLOG << "Status: " << status << FZENDL;

  // Output!
  std::string solution_string;
  for (const fz::SolutionOutputSpecs& output : model.output()) {
    solution_string.append(SolutionString(sat_model, var_map, output));
    solution_string.append("\n");
  }

  // Print the solution.
  // The "----------" is needed by minizinc.
  std::cout << solution_string << "----------" << std::endl;
}

}  // namespace sat
}  // namespace operations_research
