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

#include "flatzinc/model.h"

#include "base/hash.h"
#include <set>
#include <vector>

#include "base/join.h"
#include "base/map_util.h"
#include "base/stl_util.h"
#include "flatzinc/logging.h"

namespace operations_research {
namespace fz {
// ----- Domain -----

Domain Domain::IntegerList(std::vector<int64> values) {
  Domain result;
  result.is_interval = false;
  result.values = std::move(values);
  STLSortAndRemoveDuplicates(&result.values);
  result.display_as_boolean = false;
  return result;
}

Domain Domain::EmptyDomain() {
  Domain result;
  result.is_interval = false;
  result.display_as_boolean = false;
  return result;
}

Domain Domain::AllInt64() {
  Domain result;
  result.is_interval = true;
  result.display_as_boolean = false;
  return result;
}

Domain Domain::Singleton(int64 value) {
  Domain result;
  result.is_interval = false;
  result.values.push_back(value);
  result.display_as_boolean = false;
  return result;
}

Domain Domain::Interval(int64 included_min, int64 included_max) {
  Domain result;
  result.is_interval = true;
  result.display_as_boolean = false;
  result.values.push_back(included_min);
  result.values.push_back(included_max);
  return result;
}

Domain Domain::Boolean() {
  Domain result;
  result.is_interval = false;
  result.display_as_boolean = true;
  result.values.push_back(0);
  result.values.push_back(1);
  return result;
}

void Domain::IntersectWithDomain(const Domain& other) {
  if (other.is_interval) {
    if (!other.values.empty()) {
      IntersectWithInterval(other.values[0], other.values[1]);
    }
    return;
  }
  if (is_interval) {
    is_interval = false;  // Other is not an interval.
    if (values.empty()) {
      values = other.values;
    } else {
      const int64 imin = values[0];
      const int64 imax = values[1];
      values = other.values;
      IntersectWithInterval(imin, imax);
    }
    return;
  }
  // now deal with the intersection of two lists of values
  IntersectWithListOfIntegers(other.values);
}

void Domain::IntersectWithSingleton(int64 value) {
  IntersectWithInterval(value, value);
}

void Domain::IntersectWithInterval(int64 imin, int64 imax) {
  if (imin > imax) {  // Empty interval -> empty domain.
    is_interval = false;
    values.clear();
  } else if (is_interval) {
    if (values.empty()) {
      values.push_back(imin);
      values.push_back(imax);
    } else {
      values[0] = std::max(values[0], imin);
      values[1] = std::min(values[1], imax);
      if (values[0] > values[1]) {
        values.clear();
        is_interval = false;
      } else if (values[0] == values[1]) {
        is_interval = false;
        values.pop_back();
      }
    }
  } else {
    if (!values.empty()) {
      std::sort(values.begin(), values.end());
      std::vector<int64> new_values;
      new_values.reserve(values.size());
      for (const int64 val : values) {
        if (val > imax) break;
        if (val >= imin && (new_values.empty() || val != new_values.back())) {
          new_values.push_back(val);
        }
      }
      values.swap(new_values);
    }
  }
}

void Domain::IntersectWithListOfIntegers(const std::vector<int64>& ovalues) {
  if (is_interval) {
    const int64 dmin = values.empty() ? kint64min : values[0];
    const int64 dmax = values.empty() ? kint64max : values[1];
    values.clear();
    for (const int64 v : ovalues) {
      if (v >= dmin && v <= dmax) values.push_back(v);
    }
    STLSortAndRemoveDuplicates(&values);
    if (!values.empty() &&
        values.back() - values.front() == values.size() - 1 &&
        values.size() >= 2) {
      if (values.size() > 2) {
        // Contiguous case.
        const int64 last = values.back();
        values.resize(2);
        values[1] = last;
      }
    } else {
      // This also covers and invalid (empty) domain.
      is_interval = false;
    }
  } else {
    // TODO(user): Investigate faster code for small arrays.
    std::sort(values.begin(), values.end());
    hash_set<int64> other_values(ovalues.begin(), ovalues.end());
    std::vector<int64> new_values;
    new_values.reserve(std::min(values.size(), ovalues.size()));
    for (const int64 val : values) {
      if (ContainsKey(other_values, val) &&
          (new_values.empty() || val != new_values.back())) {
        new_values.push_back(val);
      }
    }
    values.swap(new_values);
  }
}

bool Domain::HasOneValue() const {
  return (values.size() == 1 || (values.size() == 2 && values[0] == values[1]));
}

bool Domain::empty() const { return values.size() == 0 && !is_interval; }

int64 Domain::Min() const {
  CHECK(!empty());
  return is_interval && values.empty() ? kint64min : values.front();
}

int64 Domain::Max() const {
  CHECK(!empty());
  return is_interval && values.empty() ? kint64max : values.back();
}

bool Domain::IsAllInt64() const {
  return is_interval &&
         (values.empty() || (values[0] == kint64min && values[1] == kint64max));
}

bool Domain::Contains(int64 value) const {
  if (is_interval) {
    if (values.empty()) {
      return true;
    } else {
      return value >= values[0] && value <= values[1];
    }
  } else {
    return std::find(values.begin(), values.end(), value) != values.end();
  }
}

bool Domain::RemoveValue(int64 value) {
  if (is_interval) {
    if (values.empty()) {
      return false;
    } else if (value == values[0]) {
      values[0]++;
      return true;
    } else if (value == values[1]) {
      values[1]--;
      return true;
    } else if (values[1] - values[0] < 64 && value > values[0] &&
               value < values[1]) {  // small
      const int64 vmax = values[1];
      values.pop_back();
      values.reserve(vmax - values[0]);
      for (int64 v = values[0] + 1; v <= vmax; ++v) {
        if (v != value) {
          values.push_back(v);
        }
      }
      is_interval = false;
      return true;
    }
  }
  // TODO(user): Remove value from list.
  return false;
}

std::string Domain::DebugString() const {
  if (is_interval) {
    if (values.empty()) {
      return "int";
    } else {
      return StringPrintf("[%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d]", values[0],
                          values[1]);
    }
  } else if (values.size() == 1) {
    return StringPrintf("%" GG_LL_FORMAT "d", values.back());
  } else {
    return StringPrintf("[%s]", strings::Join(values, ", ").c_str());
  }
}

// ----- Argument -----

Argument Argument::IntegerValue(int64 value) {
  Argument result;
  result.type = INT_VALUE;
  result.values.push_back(value);
  return result;
}

Argument Argument::Interval(int64 imin, int64 imax) {
  Argument result;
  result.type = INT_INTERVAL;
  result.values.push_back(imin);
  result.values.push_back(imax);
  return result;
}

Argument Argument::IntegerList(std::vector<int64> values) {
  Argument result;
  result.type = INT_LIST;
  result.values = std::move(values);
  return result;
}

Argument Argument::DomainList(std::vector<Domain> domains) {
  Argument result;
  result.type = DOMAIN_LIST;
  result.domains = std::move(domains);
  return result;
}

Argument Argument::IntVarRef(IntegerVariable* const var) {
  Argument result;
  result.type = INT_VAR_REF;
  result.variables.push_back(var);
  return result;
}

Argument Argument::IntVarRefArray(std::vector<IntegerVariable*> vars) {
  Argument result;
  result.type = INT_VAR_REF_ARRAY;
  result.variables = std::move(vars);
  return result;
}

Argument Argument::VoidArgument() {
  Argument result;
  result.type = VOID_ARGUMENT;
  return result;
}

Argument Argument::FromDomain(const Domain& domain) {
  if (domain.is_interval) {
    if (domain.values.empty()) {
      return Argument::Interval(kint64min, kint64max);
    } else {
      return Argument::Interval(domain.values[0], domain.values[1]);
    }
  } else {
    return Argument::IntegerList(domain.values);
  }
}

std::string Argument::DebugString() const {
  switch (type) {
    case INT_VALUE:
      return StringPrintf("% " GG_LL_FORMAT "d", values[0]);
    case INT_INTERVAL:
      return StringPrintf("[%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d]", values[0],
                          values[1]);
    case INT_LIST:
      return StringPrintf("[%s]", strings::Join(values, ", ").c_str());
    case DOMAIN_LIST:
      return StringPrintf("[%s]", JoinDebugString(domains, ", ").c_str());
    case INT_VAR_REF:
      return variables[0]->name;
    case INT_VAR_REF_ARRAY: {
      std::string result = "[";
      for (int i = 0; i < variables.size(); ++i) {
        result.append(variables[i]->name);
        result.append(i != variables.size() - 1 ? ", " : "]");
      }
      return result;
    }
    case VOID_ARGUMENT:
      return "VoidArgument";
  }
  LOG(FATAL) << "Unhandled case in DebugString " << static_cast<int>(type);
  return "";
}

bool Argument::IsVariable() const { return type == INT_VAR_REF; }

bool Argument::HasOneValue() const {
  return (type == INT_VALUE || (type == INT_LIST && values.size() == 1) ||
          (type == INT_INTERVAL && values[0] == values[1]) ||
          (type == INT_VAR_REF && variables[0]->domain.HasOneValue()));
}

int64 Argument::Value() const {
  DCHECK(HasOneValue()) << "Value() called on unbound Argument: "
                        << DebugString();
  switch (type) {
    case INT_VALUE:
    case INT_INTERVAL:
    case INT_LIST:
      return values[0];
    case INT_VAR_REF: {
      return variables[0]->domain.values[0];
    }
    default: { return 0; }  // Should fail anyway.
  }
}

IntegerVariable* Argument::Var() const {
  return type == INT_VAR_REF ? variables[0] : nullptr;
}

// ----- IntegerVariable -----

IntegerVariable::IntegerVariable(const std::string& name_, const Domain& domain_,
                                 bool temporary_)
    : name(name_),
      domain(domain_),
      defining_constraint(nullptr),
      temporary(temporary_),
      active(true) {
  if (!domain.is_interval) {
    STLSortAndRemoveDuplicates(&domain.values);
  }
}

bool IntegerVariable::Merge(const std::string& other_name,
                            const Domain& other_domain,
                            Constraint* const other_constraint,
                            bool other_temporary) {
  if (defining_constraint != nullptr && other_constraint != nullptr) {
    // Both are defined, we cannot merge the two variables.
    return false;
  }
  if (temporary && !other_temporary) {
    temporary = false;
    name = other_name;
  }
  if (defining_constraint == nullptr) {
    defining_constraint = other_constraint;
  }
  domain.IntersectWithDomain(other_domain);
  return true;
}

std::string IntegerVariable::DebugString() const {
  if (!domain.is_interval && domain.values.size() == 1) {
    return StringPrintf("% " GG_LL_FORMAT "d", domain.values.back());
  } else {
    return StringPrintf(
        "%s(%s%s%s)%s", name.c_str(), domain.DebugString().c_str(),
        temporary ? ", temporary" : "",
        defining_constraint != nullptr ? ", target_variable" : "",
        active ? "" : " [removed during presolve]");
  }
}

// ----- Constraint -----

std::string Constraint::DebugString() const {
  const std::string strong = strong_propagation ? "strong propagation" : "";
  const std::string presolve_status_str =
      active ? "" : (presolve_propagation_done ? "[propagated during presolve]"
                                               : "[removed during presolve]");
  const std::string target =
      target_variable != nullptr
          ? StringPrintf(" => %s", target_variable->name.c_str())
          : "";
  return StringPrintf("%s(%s)%s %s %s", type.c_str(),
                      JoinDebugString(arguments, ", ").c_str(), target.c_str(),
                      strong.c_str(), presolve_status_str.c_str());
}

void Constraint::RemoveArg(int arg_pos) {
  arguments.erase(arguments.begin() + arg_pos);
}

void Constraint::MarkAsInactive() {
  RemoveTargetVariable();
  active = false;
  // TODO(user): Reclaim arguments and memory.
}

void Constraint::SetAsFalse() {
  RemoveTargetVariable();
  type = "false_constraint";
  arguments.clear();
}

void Constraint::RemoveTargetVariable() {
  if (target_variable != nullptr) {
    if (target_variable->defining_constraint == this) {
      FZVLOG << "  - remove target_variable from " << DebugString() << FZENDL;
      target_variable->defining_constraint = nullptr;
      target_variable = nullptr;
    } else {
      FZVLOG << "  - asymmetric relation " << DebugString() << FZENDL;
      target_variable = nullptr;
    }
  }
}

// ----- Annotation -----

Annotation Annotation::Empty() {
  Annotation result;
  result.type = ANNOTATION_LIST;
  result.interval_min = 0;
  result.interval_max = 0;
  return result;
}

Annotation Annotation::AnnotationList(std::vector<Annotation> list) {
  Annotation result;
  result.type = ANNOTATION_LIST;
  result.interval_min = 0;
  result.interval_max = 0;
  result.annotations = std::move(list);
  return result;
}

Annotation Annotation::Identifier(const std::string& id) {
  Annotation result;
  result.type = IDENTIFIER;
  result.interval_min = 0;
  result.interval_max = 0;
  result.id = id;
  return result;
}

Annotation Annotation::FunctionCallWithArguments(const std::string& id,
                                                 std::vector<Annotation> args) {
  Annotation result;
  result.type = FUNCTION_CALL;
  result.interval_min = 0;
  result.interval_max = 0;
  result.id = id;
  result.annotations = std::move(args);
  return result;
}

Annotation Annotation::FunctionCall(const std::string& id) {
  Annotation result;
  result.type = FUNCTION_CALL;
  result.interval_min = 0;
  result.interval_max = 0;
  result.id = id;
  return result;
}

Annotation Annotation::Interval(int64 interval_min, int64 interval_max) {
  Annotation result;
  result.type = INTERVAL;
  result.interval_min = interval_min;
  result.interval_max = interval_max;
  return result;
}

Annotation Annotation::IntegerValue(int64 value) {
  Annotation result;
  result.type = INT_VALUE;
  result.interval_min = value;
  return result;
}

Annotation Annotation::Variable(IntegerVariable* const var) {
  Annotation result;
  result.type = INT_VAR_REF;
  result.interval_min = 0;
  result.interval_max = 0;
  result.variables.push_back(var);
  return result;
}

Annotation Annotation::VariableList(std::vector<IntegerVariable*> variables) {
  Annotation result;
  result.type = INT_VAR_REF_ARRAY;
  result.interval_min = 0;
  result.interval_max = 0;
  result.variables = std::move(variables);
  return result;
}

Annotation Annotation::String(const std::string& str) {
  Annotation result;
  result.type = STRING_VALUE;
  result.interval_min = 0;
  result.interval_max = 0;
  result.string_value = str;
  return result;
}

void Annotation::AppendAllIntegerVariables(
    std::vector<IntegerVariable*>* const vars) const {
  for (const Annotation& ann : annotations) {
    ann.AppendAllIntegerVariables(vars);
  }
  if (!variables.empty()) {
    vars->insert(vars->end(), variables.begin(), variables.end());
  }
}

std::string Annotation::DebugString() const {
  switch (type) {
    case ANNOTATION_LIST: {
      return StringPrintf("[%s]", JoinDebugString(annotations, ", ").c_str());
    }
    case IDENTIFIER: {
      return id;
    }
    case FUNCTION_CALL: {
      return StringPrintf("%s(%s)", id.c_str(),
                          JoinDebugString(annotations, ", ").c_str());
    }
    case INTERVAL: {
      return StringPrintf("%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d",
                          interval_min, interval_max);
    }
    case INT_VALUE: {
      return StringPrintf("%" GG_LL_FORMAT "d", interval_min);
    }
    case INT_VAR_REF: {
      return variables.front()->name;
    }
    case INT_VAR_REF_ARRAY: {
      std::string result = "[";
      for (int i = 0; i < variables.size(); ++i) {
        result.append(variables[i]->DebugString());
        result.append(i != variables.size() - 1 ? ", " : "]");
      }
      return result;
    }
    case STRING_VALUE: {
      return StringPrintf("\"%s\"", string_value.c_str());
    }
  }
  LOG(FATAL) << "Unhandled case in DebugString " << static_cast<int>(type);
  return "";
}

// ----- SolutionOutputSpecs -----

std::string SolutionOutputSpecs::Bounds::DebugString() const {
  return StringPrintf("%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d", min_value,
                      max_value);
}

SolutionOutputSpecs SolutionOutputSpecs::SingleVariable(
    const std::string& name, IntegerVariable* variable, bool display_as_boolean) {
  SolutionOutputSpecs result;
  result.name = name;
  result.variable = variable;
  result.display_as_boolean = display_as_boolean;
  return result;
}

SolutionOutputSpecs SolutionOutputSpecs::MultiDimensionalArray(
    const std::string& name, std::vector<Bounds> bounds,
    std::vector<IntegerVariable*> flat_variables, bool display_as_boolean) {
  SolutionOutputSpecs result;
  result.variable = nullptr;
  result.name = name;
  result.bounds = std::move(bounds);
  result.flat_variables = std::move(flat_variables);
  result.display_as_boolean = display_as_boolean;
  return result;
}

SolutionOutputSpecs SolutionOutputSpecs::VoidOutput() {
  SolutionOutputSpecs result;
  result.variable = nullptr;
  result.display_as_boolean = false;
  return result;
}

std::string SolutionOutputSpecs::DebugString() const {
  if (variable != nullptr) {
    return StringPrintf("output_var(%s)", variable->name.c_str());
  } else {
    return StringPrintf("output_array([%s] [%s])",
                        JoinDebugString(bounds, ", ").c_str(),
                        JoinNameFieldPtr(flat_variables, ", ").c_str());
  }
}

// ----- Model -----

Model::~Model() {
  STLDeleteElements(&variables_);
  STLDeleteElements(&constraints_);
}

IntegerVariable* Model::AddVariable(const std::string& name, const Domain& domain,
                                    bool defined) {
  IntegerVariable* const var = new IntegerVariable(name, domain, defined);
  variables_.push_back(var);
  return var;
}

// TODO(user): Create only once constant per value.
IntegerVariable* Model::AddConstant(int64 value) {
  IntegerVariable* const var =
      new IntegerVariable(StringPrintf("%" GG_LL_FORMAT "d", value),
                          Domain::Singleton(value), true);
  variables_.push_back(var);
  return var;
}

void Model::AddConstraint(const std::string& id, std::vector<Argument> arguments,
                          bool is_domain, IntegerVariable* defines) {
  Constraint* const constraint =
      new Constraint(id, std::move(arguments), is_domain, defines);
  constraints_.push_back(constraint);
  if (defines != nullptr) {
    defines->defining_constraint = constraint;
  }
}

void Model::AddConstraint(const std::string& id, std::vector<Argument> arguments) {
  AddConstraint(id, std::move(arguments), false, nullptr);
}

void Model::AddOutput(SolutionOutputSpecs output) {
  output_.push_back(std::move(output));
}

void Model::Satisfy(std::vector<Annotation> search_annotations) {
  objective_ = nullptr;
  search_annotations_ = std::move(search_annotations);
}

void Model::Minimize(IntegerVariable* obj,
                     std::vector<Annotation> search_annotations) {
  objective_ = obj;
  maximize_ = false;
  search_annotations_ = std::move(search_annotations);
}

void Model::Maximize(IntegerVariable* obj,
                     std::vector<Annotation> search_annotations) {
  objective_ = obj;
  maximize_ = true;
  search_annotations_ = std::move(search_annotations);
}

std::string Model::DebugString() const {
  std::string output = StringPrintf("Model %s\nVariables\n", name_.c_str());
  for (int i = 0; i < variables_.size(); ++i) {
    StringAppendF(&output, "  %s\n", variables_[i]->DebugString().c_str());
  }
  output.append("Constraints\n");
  for (int i = 0; i < constraints_.size(); ++i) {
    if (constraints_[i] != nullptr) {
      StringAppendF(&output, "  %s\n", constraints_[i]->DebugString().c_str());
    }
  }
  if (objective_ != nullptr) {
    StringAppendF(&output, "%s %s\n  %s\n", maximize_ ? "Maximize" : "Minimize",
                  objective_->name.c_str(),
                  JoinDebugString(search_annotations_, ", ").c_str());
  } else {
    StringAppendF(&output, "Satisfy\n  %s\n",
                  JoinDebugString(search_annotations_, ", ").c_str());
  }
  output.append("Output\n");
  for (int i = 0; i < output_.size(); ++i) {
    StringAppendF(&output, "  %s\n", output_[i].DebugString().c_str());
  }

  return output;
}

// ----- Model statistics -----

void ModelStatistics::PrintStatistics() const {
  FZLOG << "Model " << model_.name() << FZENDL;
  for (const auto& it : constraints_per_type_) {
    FZLOG << "  - " << it.first << ": " << it.second.size() << FZENDL;
  }
  if (model_.objective() == nullptr) {
    FZLOG << "  - Satisfaction problem" << FZENDL;
  } else {
    FZLOG << "  - " << (model_.maximize() ? "Maximization" : "Minimization")
          << " problem" << FZENDL;
  }
}

void ModelStatistics::BuildStatistics() {
  constraints_per_type_.clear();
  constraints_per_variables_.clear();
  for (Constraint* const ct : model_.constraints()) {
    if (ct != nullptr && ct->active) {
      constraints_per_type_[ct->type].push_back(ct);
      hash_set<const IntegerVariable*> marked;
      for (const Argument& arg : ct->arguments) {
        for (IntegerVariable* const var : arg.variables) {
          marked.insert(var);
        }
      }
      for (const IntegerVariable* const var : marked) {
        constraints_per_variables_[var].push_back(ct);
      }
    }
  }
}
}  // namespace fz
}  // namespace operations_research
