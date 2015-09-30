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
#include <iostream>  // NOLINT
#include <set>
#include <vector>

#include "base/join.h"
#include "base/map_util.h"
#include "base/stl_util.h"

DEFINE_bool(fz_logging, false,
            "Print logging information from the flatzinc interpreter.");
DEFINE_bool(fz_verbose, false,
            "Print verbose logging information from the flatzinc interpreter.");
DEFINE_bool(fz_debug, false,
            "Print debug logging information from the flatzinc interpreter.");


namespace operations_research {
// ----- FzDomain -----

FzDomain FzDomain::IntegerList(std::vector<int64> values) {
  FzDomain result;
  result.is_interval = false;
  result.values = std::move(values);
  result.display_as_boolean = false;
  return result;
}

FzDomain FzDomain::EmptyDomain() {
  FzDomain result;
  result.is_interval = false;
  result.display_as_boolean = false;
  return result;
}

FzDomain FzDomain::AllInt64() {
  FzDomain result;
  result.is_interval = true;
  result.display_as_boolean = false;
  return result;
}

FzDomain FzDomain::Singleton(int64 value) {
  FzDomain result;
  result.is_interval = false;
  result.values.push_back(value);
  result.display_as_boolean = false;
  return result;
}

FzDomain FzDomain::Interval(int64 included_min, int64 included_max) {
  FzDomain result;
  result.is_interval = true;
  result.display_as_boolean = false;
  result.values.push_back(included_min);
  result.values.push_back(included_max);
  return result;
}

FzDomain FzDomain::Boolean() {
  FzDomain result;
  result.is_interval = false;
  result.display_as_boolean = true;
  result.values.push_back(0);
  result.values.push_back(1);
  return result;
}

void FzDomain::IntersectWithFzDomain(const FzDomain& other) {
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

void FzDomain::IntersectWithInterval(int64 imin, int64 imax) {
  if (is_interval) {
    if (values.empty()) {
      values.push_back(imin);
      values.push_back(imax);
    } else {
      values[0] = std::max(values[0], imin);
      values[1] = std::min(values[1], imax);
    }
  } else {
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

void FzDomain::IntersectWithListOfIntegers(const std::vector<int64>& ovalues) {
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

bool FzDomain::IsSingleton() const {
  return (values.size() == 1 || (values.size() == 2 && values[0] == values[1]));
}

bool FzDomain::Contains(int64 value) const {
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

bool FzDomain::RemoveValue(int64 value) {
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

std::string FzDomain::DebugString() const {
  if (is_interval) {
    if (values.empty()) {
      return "int";
    } else {
      return StringPrintf("[%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d]", values[0],
                          values[1]);
    }
  } else if (values.size() == 1) {
    return StringPrintf("% " GG_LL_FORMAT "d", values.back());
  } else {
    return StringPrintf("[%s]", strings::Join(values, ", ").c_str());
  }
}

// ----- FzArgument -----

FzArgument FzArgument::IntegerValue(int64 value) {
  FzArgument result;
  result.type = INT_VALUE;
  result.values.push_back(value);
  return result;
}

FzArgument FzArgument::Interval(int64 imin, int64 imax) {
  FzArgument result;
  result.type = INT_INTERVAL;
  result.values.push_back(imin);
  result.values.push_back(imax);
  return result;
}

FzArgument FzArgument::IntegerList(std::vector<int64> values) {
  FzArgument result;
  result.type = INT_LIST;
  result.values = std::move(values);
  return result;
}

FzArgument FzArgument::DomainList(std::vector<FzDomain> domains) {
  FzArgument result;
  result.type = DOMAIN_LIST;
  result.domains = std::move(domains);
  return result;
}

FzArgument FzArgument::IntVarRef(FzIntegerVariable* const var) {
  FzArgument result;
  result.type = INT_VAR_REF;
  result.variables.push_back(var);
  return result;
}

FzArgument FzArgument::IntVarRefArray(std::vector<FzIntegerVariable*> vars) {
  FzArgument result;
  result.type = INT_VAR_REF_ARRAY;
  result.variables = std::move(vars);
  return result;
}

FzArgument FzArgument::VoidArgument() {
  FzArgument result;
  result.type = VOID_ARGUMENT;
  return result;
}

FzArgument FzArgument::FromDomain(const FzDomain& domain) {
  if (domain.is_interval) {
    if (domain.values.empty()) {
      return FzArgument::Interval(kint64min, kint64max);
    } else {
      return FzArgument::Interval(domain.values[0], domain.values[1]);
    }
  } else {
    return FzArgument::IntegerList(domain.values);
  }
}

std::string FzArgument::DebugString() const {
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

bool FzArgument::IsVariable() const { return type == INT_VAR_REF; }

bool FzArgument::HasOneValue() const {
  return (type == INT_VALUE || (type == INT_LIST && values.size() == 1) ||
          (type == INT_INTERVAL && values[0] == values[1]) ||
          (type == INT_VAR_REF && variables[0]->domain.IsSingleton()));
}

int64 FzArgument::Value() const {
  DCHECK(HasOneValue()) << "Value() called on unbound FzArgument: "
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

FzIntegerVariable* FzArgument::Var() const {
  return type == INT_VAR_REF ? variables[0] : nullptr;
}

// ----- FzIntegerVariable -----

FzIntegerVariable::FzIntegerVariable(const std::string& name_,
                                     const FzDomain& domain_, bool temporary_)
    : name(name_),
      domain(domain_),
      defining_constraint(nullptr),
      temporary(temporary_),
      active(true) {
  if (!domain.is_interval) {
    STLSortAndRemoveDuplicates(&domain.values);
  }
}

bool FzIntegerVariable::Merge(const std::string& other_name,
                              const FzDomain& other_domain,
                              FzConstraint* const other_constraint,
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
  domain.IntersectWithFzDomain(other_domain);
  return true;
}

int64 FzIntegerVariable::Min() const {
  return domain.is_interval && domain.values.empty() ? kint64min
                                                     : domain.values.front();
}

int64 FzIntegerVariable::Max() const {
  return domain.is_interval && domain.values.empty() ? kint64max
                                                     : domain.values.back();
}

bool FzIntegerVariable::IsAllInt64() const {
  return domain.is_interval &&
         (domain.values.empty() ||
          (domain.values[0] == kint64min && domain.values[1] == kint64max));
}

bool FzIntegerVariable::HasOneValue() const { return domain.IsSingleton(); }

std::string FzIntegerVariable::DebugString() const {
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

// ----- FzConstraint -----

std::string FzConstraint::DebugString() const {
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

void FzConstraint::MarkAsInactive() {
  RemoveTargetVariable();
  FZVLOG << "  - marking " << DebugString() << " as inactive" << FZENDL;
  active = false;
  // TODO(user): Reclaim arguments and memory.
}

void FzConstraint::RemoveTargetVariable() {
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

// ----- FzAnnotation -----

FzAnnotation FzAnnotation::Empty() {
  FzAnnotation result;
  result.type = ANNOTATION_LIST;
  result.interval_min = 0;
  result.interval_max = 0;
  return result;
}

FzAnnotation FzAnnotation::AnnotationList(std::vector<FzAnnotation> list) {
  FzAnnotation result;
  result.type = ANNOTATION_LIST;
  result.interval_min = 0;
  result.interval_max = 0;
  result.annotations = std::move(list);
  return result;
}

FzAnnotation FzAnnotation::Identifier(const std::string& id) {
  FzAnnotation result;
  result.type = IDENTIFIER;
  result.interval_min = 0;
  result.interval_max = 0;
  result.id = id;
  return result;
}

FzAnnotation FzAnnotation::FunctionCallWithArguments(
    const std::string& id, std::vector<FzAnnotation> args) {
  FzAnnotation result;
  result.type = FUNCTION_CALL;
  result.interval_min = 0;
  result.interval_max = 0;
  result.id = id;
  result.annotations = std::move(args);
  return result;
}

FzAnnotation FzAnnotation::FunctionCall(const std::string& id) {
  FzAnnotation result;
  result.type = FUNCTION_CALL;
  result.interval_min = 0;
  result.interval_max = 0;
  result.id = id;
  return result;
}

FzAnnotation FzAnnotation::Interval(int64 interval_min, int64 interval_max) {
  FzAnnotation result;
  result.type = INTERVAL;
  result.interval_min = interval_min;
  result.interval_max = interval_max;
  return result;
}

FzAnnotation FzAnnotation::IntegerValue(int64 value) {
  FzAnnotation result;
  result.type = INT_VALUE;
  result.interval_min = value;
  return result;
}

FzAnnotation FzAnnotation::Variable(FzIntegerVariable* const var) {
  FzAnnotation result;
  result.type = INT_VAR_REF;
  result.interval_min = 0;
  result.interval_max = 0;
  result.variables.push_back(var);
  return result;
}

FzAnnotation FzAnnotation::VariableList(
    std::vector<FzIntegerVariable*> variables) {
  FzAnnotation result;
  result.type = INT_VAR_REF_ARRAY;
  result.interval_min = 0;
  result.interval_max = 0;
  result.variables = std::move(variables);
  return result;
}

FzAnnotation FzAnnotation::String(const std::string& str) {
  FzAnnotation result;
  result.type = STRING_VALUE;
  result.interval_min = 0;
  result.interval_max = 0;
  result.string_value_ = str;
  return result;
}

void FzAnnotation::GetAllIntegerVariables(
    std::vector<FzIntegerVariable*>* const vars) const {
  for (const FzAnnotation& ann : annotations) {
    ann.GetAllIntegerVariables(vars);
  }
  if (!variables.empty()) {
    vars->insert(vars->end(), variables.begin(), variables.end());
  }
}

std::string FzAnnotation::DebugString() const {
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
      return StringPrintf("\"%s\"", string_value_.c_str());
    }
  }
  LOG(FATAL) << "Unhandled case in DebugString " << static_cast<int>(type);
  return "";
}

// ----- FzOnSolutionOutput -----

std::string FzOnSolutionOutput::Bounds::DebugString() const {
  return StringPrintf("%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d", min_value,
                      max_value);
}

FzOnSolutionOutput FzOnSolutionOutput::SingleVariable(
    const std::string& name, FzIntegerVariable* const variable,
    bool display_as_boolean) {
  FzOnSolutionOutput result;
  result.name = name;
  result.variable = variable;
  result.display_as_boolean = display_as_boolean;
  return result;
}

FzOnSolutionOutput FzOnSolutionOutput::MultiDimensionalArray(
    const std::string& name, std::vector<Bounds> bounds,
    std::vector<FzIntegerVariable*> flat_variables, bool display_as_boolean) {
  FzOnSolutionOutput result;
  result.variable = nullptr;
  result.name = name;
  result.bounds = std::move(bounds);
  result.flat_variables = std::move(flat_variables);
  result.display_as_boolean = display_as_boolean;
  return result;
}

FzOnSolutionOutput FzOnSolutionOutput::VoidOutput() {
  FzOnSolutionOutput result;
  result.variable = nullptr;
  result.display_as_boolean = false;
  return result;
}

std::string FzOnSolutionOutput::DebugString() const {
  if (variable != nullptr) {
    return StringPrintf("output_var(%s)", variable->name.c_str());
  } else {
    return StringPrintf("output_array([%s] [%s])",
                        JoinDebugString(bounds, ", ").c_str(),
                        JoinNameFieldPtr(flat_variables, ", ").c_str());
  }
}

// ----- FzModel -----

FzModel::~FzModel() {
  STLDeleteElements(&variables_);
  STLDeleteElements(&constraints_);
}

FzIntegerVariable* FzModel::AddVariable(const std::string& name,
                                        const FzDomain& domain, bool defined) {
  FzIntegerVariable* const var = new FzIntegerVariable(name, domain, defined);
  variables_.push_back(var);
  return var;
}

void FzModel::AddConstraint(const std::string& id,
                            std::vector<FzArgument> arguments, bool is_domain,
                            FzIntegerVariable* const defines) {
  FzConstraint* const constraint =
      new FzConstraint(id, std::move(arguments), is_domain, defines);
  constraints_.push_back(constraint);
  if (defines != nullptr) {
    defines->defining_constraint = constraint;
  }
}

void FzModel::AddOutput(const FzOnSolutionOutput& output) {
  output_.push_back(output);
}

void FzModel::Satisfy(std::vector<FzAnnotation> search_annotations) {
  objective_ = nullptr;
  search_annotations_ = std::move(search_annotations);
}

void FzModel::Minimize(FzIntegerVariable* obj,
                       std::vector<FzAnnotation> search_annotations) {
  objective_ = obj;
  maximize_ = false;
  search_annotations_ = std::move(search_annotations);
}

void FzModel::Maximize(FzIntegerVariable* obj,
                       std::vector<FzAnnotation> search_annotations) {
  objective_ = obj;
  maximize_ = true;
  search_annotations_ = std::move(search_annotations);
}

std::string FzModel::DebugString() const {
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

void FzModelStatistics::PrintStatistics() {
  BuildStatistics();
  FZLOG << "Model " << model_.name() << FZENDL;
  BuildStatistics();
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

void FzModelStatistics::BuildStatistics() {
  constraints_per_type_.clear();
  constraints_per_variables_.clear();
  for (FzConstraint* const ct : model_.constraints()) {
    if (ct != nullptr && ct->active) {
      constraints_per_type_[ct->type].push_back(ct);
      hash_set<const FzIntegerVariable*> marked;
      for (const FzArgument& arg : ct->arguments) {
        for (FzIntegerVariable* const var : arg.variables) {
          marked.insert(var);
        }
      }
      for (const FzIntegerVariable* const var : marked) {
        constraints_per_variables_[var].push_back(ct);
      }
    }
  }
}
}  // namespace operations_research
