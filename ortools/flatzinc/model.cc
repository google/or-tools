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

#include "ortools/flatzinc/model.h"

#include <cstdint>
#include <limits>
#include <set>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace fz {
// ----- Domain -----

Domain Domain::IntegerList(std::vector<int64_t> values) {
  Domain result;
  result.values = std::move(values);
  gtl::STLSortAndRemoveDuplicates(&result.values);
  return result;
}

Domain Domain::AllInt64() {
  Domain result;
  result.is_interval = true;
  return result;
}

Domain Domain::IntegerValue(int64_t value) {
  Domain result;
  result.values.push_back(value);
  return result;
}

Domain Domain::Interval(int64_t included_min, int64_t included_max) {
  Domain result;
  result.is_interval = true;
  result.values.push_back(included_min);
  result.values.push_back(included_max);
  return result;
}

Domain Domain::Boolean() {
  Domain result;
  result.display_as_boolean = true;
  result.values.push_back(0);
  result.values.push_back(1);
  return result;
}

Domain Domain::SetOfIntegerList(std::vector<int64_t> values) {
  Domain result = IntegerList(std::move(values));
  result.is_a_set = true;
  return result;
}

Domain Domain::SetOfAllInt64() {
  Domain result = AllInt64();
  result.is_a_set = true;
  return result;
}

Domain Domain::SetOfIntegerValue(int64_t value) {
  Domain result = IntegerValue(value);
  result.is_a_set = true;
  return result;
}

Domain Domain::SetOfInterval(int64_t included_min, int64_t included_max) {
  Domain result = Interval(included_min, included_max);
  result.is_a_set = true;
  return result;
}

Domain Domain::SetOfBoolean() {
  Domain result = Boolean();
  result.is_a_set = true;
  return result;
}

Domain Domain::EmptyDomain() { return Domain(); }

Domain Domain::AllFloats() {
  Domain result;
  result.is_interval = true;
  result.is_float = true;
  return result;
}

Domain Domain::FloatInterval(double lb, double ub) {
  Domain result;
  result.is_interval = true;
  result.is_float = true;
  result.float_values = {lb, ub};
  return result;
}

Domain Domain::FloatValue(double value) {
  Domain result;
  result.is_float = true;
  result.float_values.push_back(value);
  return result;
}

bool Domain::IntersectWithDomain(const Domain& domain) {
  if (is_float) {
    return IntersectWithFloatDomain(domain);
  }
  if (domain.is_interval) {
    if (!domain.values.empty()) {
      return IntersectWithInterval(domain.values[0], domain.values[1]);
    }
    return false;
  }
  if (is_interval) {
    is_interval = false;  // Other is not an interval.
    if (values.empty()) {
      values = domain.values;
    } else {
      const int64_t imin = values[0];
      const int64_t imax = values[1];
      values = domain.values;
      IntersectWithInterval(imin, imax);
    }
    return true;
  }
  // now deal with the intersection of two lists of values
  return IntersectWithListOfIntegers(domain.values);
}

bool Domain::IntersectWithSingleton(int64_t value) {
  return IntersectWithInterval(value, value);
}

bool Domain::IntersectWithInterval(int64_t interval_min, int64_t interval_max) {
  if (interval_min > interval_max) {  // Empty interval -> empty domain.
    is_interval = false;
    values.clear();
    return true;
  } else if (is_interval) {
    if (values.empty()) {
      values.push_back(interval_min);
      values.push_back(interval_max);
      return true;
    } else {
      if (values[0] >= interval_min && values[1] <= interval_max) return false;
      values[0] = std::max(values[0], interval_min);
      values[1] = std::min(values[1], interval_max);
      if (values[0] > values[1]) {
        values.clear();
        is_interval = false;
      } else if (values[0] == values[1]) {
        is_interval = false;
        values.pop_back();
      }
      return true;
    }
  } else {
    if (!values.empty()) {
      std::sort(values.begin(), values.end());
      std::vector<int64_t> new_values;
      new_values.reserve(values.size());
      bool changed = false;
      for (const int64_t val : values) {
        if (val > interval_max) {
          changed = true;
          break;
        }
        if (val >= interval_min &&
            (new_values.empty() || val != new_values.back())) {
          new_values.push_back(val);
        } else {
          changed = true;
        }
      }
      values.swap(new_values);
      return changed;
    }
  }
  return false;
}

bool Domain::IntersectWithListOfIntegers(const std::vector<int64_t>& integers) {
  if (is_interval) {
    const int64_t dmin =
        values.empty() ? std::numeric_limits<int64_t>::min() : values[0];
    const int64_t dmax =
        values.empty() ? std::numeric_limits<int64_t>::max() : values[1];
    values.clear();
    for (const int64_t v : integers) {
      if (v >= dmin && v <= dmax) values.push_back(v);
    }
    gtl::STLSortAndRemoveDuplicates(&values);
    if (!values.empty() &&
        values.back() - values.front() == values.size() - 1 &&
        values.size() >= 2) {
      if (values.size() > 2) {
        // Contiguous case.
        const int64_t last = values.back();
        values.resize(2);
        values[1] = last;
      }
      return values[0] != dmin || values[1] != dmax;
    } else {
      // This also covers and invalid (empty) domain.
      is_interval = false;
      return true;
    }
  } else {
    // TODO(user): Investigate faster code for small arrays.
    std::sort(values.begin(), values.end());
    absl::flat_hash_set<int64_t> other_values(integers.begin(), integers.end());
    std::vector<int64_t> new_values;
    new_values.reserve(std::min(values.size(), integers.size()));
    bool changed = false;
    for (const int64_t val : values) {
      if (other_values.contains(val)) {
        if (new_values.empty() || val != new_values.back()) {
          new_values.push_back(val);
        }
      } else {
        changed = true;
      }
    }
    values.swap(new_values);
    return changed;
  }
}

bool Domain::IntersectWithFloatDomain(const Domain& domain) {
  CHECK(domain.is_float);
  if (!is_interval && float_values.empty()) {
    // Empty domain. Nothing to do.
    return false;
  }
  if (!domain.is_interval && domain.float_values.empty()) {
    return SetEmptyFloatDomain();
  }
  if (domain.is_interval && domain.float_values.empty()) {
    // domain is all floats. Nothing to do.
    return false;
  }

  if (is_interval && float_values.empty()) {  // Currently all floats.
    // Copy the domain.
    is_interval = domain.is_interval;
    float_values = domain.float_values;
    return true;
  }

  if (is_interval) {
    // this is a double interval.
    CHECK_EQ(2, float_values.size());
    if (domain.is_interval) {
      bool changed = false;
      if (float_values[0] < domain.float_values[0]) {
        float_values[0] = domain.float_values[0];
        changed = true;
      }
      if (float_values[1] > domain.float_values[1]) {
        float_values[1] = domain.float_values[1];
        changed = true;
      }
      if (float_values[0] > float_values[1]) {
        return SetEmptyFloatDomain();
      }
      return changed;
    } else {
      CHECK_EQ(1, domain.float_values.size());
      const double value = domain.float_values[0];
      if (value >= float_values[0] && value <= float_values[1]) {
        is_interval = false;
        float_values = {value};
        return true;
      }
      return SetEmptyFloatDomain();
    }
  } else {
    // this is a single double.
    CHECK_EQ(1, float_values.size());
    const double value = float_values[0];
    if (domain.is_interval) {
      CHECK_EQ(2, domain.float_values.size());
      if (value >= domain.float_values[0] && value <= domain.float_values[1]) {
        // value is compatible with domain.
        return true;
      }
      return SetEmptyFloatDomain();
    } else {
      CHECK_EQ(1, domain.float_values.size());
      if (value == domain.float_values[0]) {
        // Same value;
        return true;
      }
      return SetEmptyFloatDomain();
    }
  }
}

bool Domain::SetEmptyFloatDomain() {
  CHECK(is_float);
  is_interval = false;
  float_values.clear();
  return true;
}

bool Domain::HasOneValue() const {
  return (values.size() == 1 || (values.size() == 2 && values[0] == values[1]));
}

bool Domain::empty() const {
  return is_interval ? (values.size() == 2 && values[0] > values[1])
                     : values.empty();
}

int64_t Domain::Min() const {
  CHECK(!empty());
  return is_interval && values.empty() ? std::numeric_limits<int64_t>::min()
                                       : values.front();
}

int64_t Domain::Max() const {
  CHECK(!empty());
  return is_interval && values.empty() ? std::numeric_limits<int64_t>::max()
                                       : values.back();
}

int64_t Domain::Value() const {
  CHECK(HasOneValue());
  return values.front();
}

bool Domain::IsAllInt64() const {
  return is_interval &&
         (values.empty() || (values[0] == std::numeric_limits<int64_t>::min() &&
                             values[1] == std::numeric_limits<int64_t>::max()));
}

bool Domain::Contains(int64_t value) const {
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

namespace {
bool IntervalOverlapValues(int64_t lb, int64_t ub,
                           const std::vector<int64_t>& values) {
  for (int64_t value : values) {
    if (lb <= value && value <= ub) {
      return true;
    }
  }
  return false;
}
}  // namespace

bool Domain::OverlapsIntList(const std::vector<int64_t>& vec) const {
  if (IsAllInt64()) {
    return true;
  }
  if (is_interval) {
    CHECK(!values.empty());
    return IntervalOverlapValues(values[0], values[1], vec);
  } else {
    // TODO(user): Better algorithm, sort and compare increasingly.
    const std::vector<int64_t>& to_scan =
        values.size() <= vec.size() ? values : vec;
    const absl::flat_hash_set<int64_t> container =
        values.size() <= vec.size()
            ? absl::flat_hash_set<int64_t>(vec.begin(), vec.end())
            : absl::flat_hash_set<int64_t>(values.begin(), values.end());
    for (int64_t value : to_scan) {
      if (container.contains(value)) {
        return true;
      }
    }
    return false;
  }
}

bool Domain::OverlapsIntInterval(int64_t lb, int64_t ub) const {
  if (IsAllInt64()) {
    return true;
  }
  if (is_interval) {
    CHECK(!values.empty());
    const int64_t dlb = values[0];
    const int64_t dub = values[1];
    return !(dub < lb || dlb > ub);
  } else {
    return IntervalOverlapValues(lb, ub, values);
  }
}

bool Domain::OverlapsDomain(const Domain& other) const {
  if (other.is_interval) {
    if (other.values.empty()) {
      return true;
    } else {
      return OverlapsIntInterval(other.values[0], other.values[1]);
    }
  } else {
    return OverlapsIntList(other.values);
  }
}

bool Domain::RemoveValue(int64_t value) {
  if (is_interval) {
    if (values.empty()) {
      return false;
    } else if (value == values[0] && value != values[1]) {
      values[0]++;
      return true;
    } else if (value == values[1] && value != values[0]) {
      values[1]--;
      return true;
    } else if (values[1] - values[0] < 1024 && value > values[0] &&
               value < values[1]) {  // small
      const int64_t vmax = values[1];
      values.pop_back();
      values.reserve(vmax - values[0]);
      for (int64_t v = values[0] + 1; v <= vmax; ++v) {
        if (v != value) {
          values.push_back(v);
        }
      }
      is_interval = false;
      return true;
    }
  } else {
    values.erase(std::remove(values.begin(), values.end(), value),
                 values.end());
    return true;
  }
  return false;
}

std::string Domain::DebugString() const {
  if (is_float) {
    switch (float_values.size()) {
      case 0:
        return "float";
      case 1:
        return absl::StrCat(float_values[0]);
      case 2:
        return absl::StrCat("[", float_values[0], "..", float_values[1], "]");
      default:
        LOG(DFATAL) << "Error with float domain";
        return "error_float";
    }
  }
  if (is_interval) {
    if (values.empty()) {
      return "int";
    } else {
      return absl::StrFormat("[%d..%d]", values[0], values[1]);
    }
  } else if (values.size() == 1) {
    return absl::StrCat(values.back());
  } else {
    return absl::StrFormat("[%s]", absl::StrJoin(values, ", "));
  }
}

// ----- Argument -----

Argument Argument::IntegerValue(int64_t value) {
  Argument result;
  result.type = INT_VALUE;
  result.values.push_back(value);
  return result;
}

Argument Argument::Interval(int64_t imin, int64_t imax) {
  Argument result;
  result.type = INT_INTERVAL;
  result.values.push_back(imin);
  result.values.push_back(imax);
  return result;
}

Argument Argument::IntegerList(std::vector<int64_t> values) {
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

Argument Argument::VarRef(Variable* const var) {
  Argument result;
  result.type = VAR_REF;
  result.variables.push_back(var);
  return result;
}

Argument Argument::VarRefArray(std::vector<Variable*> vars) {
  Argument result;
  result.type = VAR_REF_ARRAY;
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
      return Argument::Interval(std::numeric_limits<int64_t>::min(),
                                std::numeric_limits<int64_t>::max());
    } else {
      return Argument::Interval(domain.values[0], domain.values[1]);
    }
  } else {
    return Argument::IntegerList(domain.values);
  }
}

Argument Argument::FloatValue(double value) {
  Argument result;
  result.type = FLOAT_VALUE;
  result.floats.push_back(value);
  return result;
}

Argument Argument::FloatInterval(double lb, double ub) {
  Argument result;
  result.type = FLOAT_INTERVAL;
  result.floats.push_back(lb);
  result.floats.push_back(ub);
  return result;
}

Argument Argument::FloatList(std::vector<double> floats) {
  Argument result;
  result.type = FLOAT_LIST;
  result.floats = std::move(floats);
  return result;
}

std::string Argument::DebugString() const {
  switch (type) {
    case INT_VALUE:
      return absl::StrFormat("%d", values[0]);
    case INT_INTERVAL:
      return absl::StrFormat("[%d..%d]", values[0], values[1]);
    case INT_LIST:
      return absl::StrFormat("[%s]", absl::StrJoin(values, ", "));
    case DOMAIN_LIST:
      return absl::StrFormat("[%s]", JoinDebugString(domains, ", "));
    case VAR_REF:
      return variables[0]->name;
    case VAR_REF_ARRAY: {
      std::string result = "[";
      for (int i = 0; i < variables.size(); ++i) {
        result.append(variables[i]->name);
        result.append(i != variables.size() - 1 ? ", " : "]");
      }
      return result;
    }
    case VOID_ARGUMENT:
      return "VoidArgument";
    case FLOAT_VALUE:
      return absl::StrCat(floats[0]);
    case FLOAT_INTERVAL:
      return absl::StrCat("[", floats[0], "..", floats[1], "]");
    case FLOAT_LIST:
      return absl::StrFormat("[%s]", absl::StrJoin(floats, ", "));
  }
  LOG(FATAL) << "Unhandled case in DebugString " << static_cast<int>(type);
  return "";
}

bool Argument::IsVariable() const { return type == VAR_REF; }

bool Argument::HasOneValue() const {
  return (type == INT_VALUE || (type == INT_LIST && values.size() == 1) ||
          (type == INT_INTERVAL && values[0] == values[1]) ||
          (type == VAR_REF && variables[0]->domain.HasOneValue()));
}

int64_t Argument::Value() const {
  DCHECK(HasOneValue()) << "Value() called on unbound Argument: "
                        << DebugString();
  switch (type) {
    case INT_VALUE:
    case INT_INTERVAL:
    case INT_LIST:
      return values[0];
    case VAR_REF: {
      return variables[0]->domain.values[0];
    }
    default: {
      LOG(FATAL) << "Should not be here";
      return 0;
    }
  }
}

bool Argument::IsArrayOfValues() const {
  switch (type) {
    case INT_VALUE:
      return false;
    case INT_INTERVAL:
      return false;
    case INT_LIST:
      return true;
    case DOMAIN_LIST: {
      for (const Domain& domain : domains) {
        if (!domain.HasOneValue()) {
          return false;
        }
      }
      return true;
    }
    case VAR_REF:
      return false;
    case VAR_REF_ARRAY: {
      for (Variable* var : variables) {
        if (!var->domain.HasOneValue()) {
          return false;
        }
      }
      return true;
    }
    case VOID_ARGUMENT:
      return false;
    case FLOAT_VALUE:
      return false;
    case FLOAT_INTERVAL:
      return false;
    case FLOAT_LIST:
      return false;
  }
}

bool Argument::Contains(int64_t value) const {
  switch (type) {
    case Argument::INT_LIST: {
      return std::find(values.begin(), values.end(), value) != values.end();
    }
    case Argument::INT_INTERVAL: {
      return value >= values.front() && value <= values.back();
    }
    case Argument::INT_VALUE: {
      return value == values.front();
    }
    default: {
      LOG(FATAL) << "Cannot call Contains() on " << DebugString();
      return false;
    }
  }
}

int64_t Argument::ValueAt(int pos) const {
  switch (type) {
    case INT_LIST:
      CHECK_GE(pos, 0);
      CHECK_LT(pos, values.size());
      return values[pos];
    case DOMAIN_LIST: {
      CHECK_GE(pos, 0);
      CHECK_LT(pos, domains.size());
      CHECK(domains[pos].HasOneValue());
      return domains[pos].Value();
    }
    case VAR_REF_ARRAY: {
      CHECK_GE(pos, 0);
      CHECK_LT(pos, variables.size());
      CHECK(variables[pos]->domain.HasOneValue());
      return variables[pos]->domain.Value();
    }
    default: {
      LOG(FATAL) << "Should not be here";
      return 0;
    }
  }
}

Variable* Argument::Var() const {
  return type == VAR_REF ? variables[0] : nullptr;
}

Variable* Argument::VarAt(int pos) const {
  return type == VAR_REF_ARRAY ? variables[pos] : nullptr;
}

// ----- Variable -----

Variable::Variable(const std::string& name_, const Domain& domain_,
                   bool temporary_)
    : name(name_), domain(domain_), temporary(temporary_), active(true) {
  if (!domain.is_interval) {
    gtl::STLSortAndRemoveDuplicates(&domain.values);
  }
}

bool Variable::Merge(const std::string& other_name, const Domain& other_domain,
                     bool other_temporary) {
  if (temporary && !other_temporary) {
    temporary = false;
    name = other_name;
  }
  domain.IntersectWithDomain(other_domain);
  return true;
}

std::string Variable::DebugString() const {
  if (!domain.is_interval && domain.values.size() == 1) {
    return absl::StrFormat("% d", domain.values.back());
  } else {
    return absl::StrFormat("%s(%s%s)%s", name, domain.DebugString(),
                           temporary ? ", temporary" : "",
                           active ? "" : " [removed during presolve]");
  }
}

// ----- Constraint -----

std::string Constraint::DebugString() const {
  const std::string strong = strong_propagation ? "strong propagation" : "";
  const std::string presolve_status_str =
      active ? ""
             : (presolve_propagation_done ? "[propagated during presolve]"
                                          : "[removed during presolve]");
  return absl::StrFormat("%s(%s)%s %s", type, JoinDebugString(arguments, ", "),
                         strong, presolve_status_str);
}

void Constraint::RemoveArg(int arg_pos) {
  arguments.erase(arguments.begin() + arg_pos);
}

void Constraint::MarkAsInactive() {
  active = false;
  // TODO(user): Reclaim arguments and memory.
}

void Constraint::SetAsFalse() {
  type = "false_constraint";
  arguments.clear();
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

Annotation Annotation::Interval(int64_t interval_min, int64_t interval_max) {
  Annotation result;
  result.type = INTERVAL;
  result.interval_min = interval_min;
  result.interval_max = interval_max;
  return result;
}

Annotation Annotation::IntegerValue(int64_t value) {
  Annotation result;
  result.type = INT_VALUE;
  result.interval_min = value;
  return result;
}

Annotation Annotation::VarRef(Variable* const var) {
  Annotation result;
  result.type = VAR_REF;
  result.interval_min = 0;
  result.interval_max = 0;
  result.variables.push_back(var);
  return result;
}

Annotation Annotation::VarRefArray(std::vector<Variable*> variables) {
  Annotation result;
  result.type = VAR_REF_ARRAY;
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

void Annotation::AppendAllVariables(std::vector<Variable*>* const vars) const {
  for (const Annotation& ann : annotations) {
    ann.AppendAllVariables(vars);
  }
  if (!variables.empty()) {
    vars->insert(vars->end(), variables.begin(), variables.end());
  }
}

std::string Annotation::DebugString() const {
  switch (type) {
    case ANNOTATION_LIST: {
      return absl::StrFormat("[%s]", JoinDebugString(annotations, ", "));
    }
    case IDENTIFIER: {
      return id;
    }
    case FUNCTION_CALL: {
      return absl::StrFormat("%s(%s)", id, JoinDebugString(annotations, ", "));
    }
    case INTERVAL: {
      return absl::StrFormat("%d..%d", interval_min, interval_max);
    }
    case INT_VALUE: {
      return absl::StrCat(interval_min);
    }
    case VAR_REF: {
      return variables.front()->name;
    }
    case VAR_REF_ARRAY: {
      std::string result = "[";
      for (int i = 0; i < variables.size(); ++i) {
        result.append(variables[i]->DebugString());
        result.append(i != variables.size() - 1 ? ", " : "]");
      }
      return result;
    }
    case STRING_VALUE: {
      return absl::StrFormat("\"%s\"", string_value);
    }
  }
  LOG(FATAL) << "Unhandled case in DebugString " << static_cast<int>(type);
  return "";
}

// ----- SolutionOutputSpecs -----

std::string SolutionOutputSpecs::Bounds::DebugString() const {
  return absl::StrFormat("%d..%d", min_value, max_value);
}

SolutionOutputSpecs SolutionOutputSpecs::SingleVariable(
    const std::string& name, Variable* variable, bool display_as_boolean) {
  SolutionOutputSpecs result;
  result.name = name;
  result.variable = variable;
  result.display_as_boolean = display_as_boolean;
  return result;
}

SolutionOutputSpecs SolutionOutputSpecs::MultiDimensionalArray(
    const std::string& name, std::vector<Bounds> bounds,
    std::vector<Variable*> flat_variables, bool display_as_boolean) {
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
    return absl::StrFormat("output_var(%s)", variable->name);
  } else {
    return absl::StrFormat("output_array([%s] [%s])",
                           JoinDebugString(bounds, ", "),
                           JoinNameFieldPtr(flat_variables, ", "));
  }
}

// ----- Model -----

Model::~Model() {
  gtl::STLDeleteElements(&variables_);
  gtl::STLDeleteElements(&constraints_);
}

Variable* Model::AddVariable(const std::string& name, const Domain& domain,
                             bool defined) {
  Variable* const var = new Variable(name, domain, defined);
  variables_.push_back(var);
  return var;
}

// TODO(user): Create only once constant per value.
Variable* Model::AddConstant(int64_t value) {
  Variable* const var =
      new Variable(absl::StrCat(value), Domain::IntegerValue(value), true);
  variables_.push_back(var);
  return var;
}

Variable* Model::AddFloatConstant(double value) {
  Variable* const var =
      new Variable(absl::StrCat(value), Domain::FloatValue(value), true);
  variables_.push_back(var);
  return var;
}

void Model::AddConstraint(const std::string& id,
                          std::vector<Argument> arguments, bool is_domain) {
  Constraint* const constraint =
      new Constraint(id, std::move(arguments), is_domain);
  constraints_.push_back(constraint);
}

void Model::AddConstraint(const std::string& id,
                          std::vector<Argument> arguments) {
  AddConstraint(id, std::move(arguments), false);
}

void Model::AddOutput(SolutionOutputSpecs output) {
  output_.push_back(std::move(output));
}

void Model::Satisfy(std::vector<Annotation> search_annotations) {
  objective_ = nullptr;
  search_annotations_ = std::move(search_annotations);
}

void Model::Minimize(Variable* obj,
                     std::vector<Annotation> search_annotations) {
  objective_ = obj;
  maximize_ = false;
  search_annotations_ = std::move(search_annotations);
}

void Model::Maximize(Variable* obj,
                     std::vector<Annotation> search_annotations) {
  objective_ = obj;
  maximize_ = true;
  search_annotations_ = std::move(search_annotations);
}

std::string Model::DebugString() const {
  std::string output = absl::StrFormat("Model %s\nVariables\n", name_);
  for (int i = 0; i < variables_.size(); ++i) {
    absl::StrAppendFormat(&output, "  %s\n", variables_[i]->DebugString());
  }
  output.append("Constraints\n");
  for (int i = 0; i < constraints_.size(); ++i) {
    if (constraints_[i] != nullptr) {
      absl::StrAppendFormat(&output, "  %s\n", constraints_[i]->DebugString());
    }
  }
  if (objective_ != nullptr) {
    absl::StrAppendFormat(&output, "%s %s\n  %s\n",
                          maximize_ ? "Maximize" : "Minimize", objective_->name,
                          JoinDebugString(search_annotations_, ", "));
  } else {
    absl::StrAppendFormat(&output, "Satisfy\n  %s\n",
                          JoinDebugString(search_annotations_, ", "));
  }
  output.append("Output\n");
  for (int i = 0; i < output_.size(); ++i) {
    absl::StrAppendFormat(&output, "  %s\n", output_[i].DebugString());
  }

  return output;
}

bool Model::IsInconsistent() const {
  for (Variable* var : variables_) {
    if (var->domain.empty()) {
      return true;
    }
  }
  for (Constraint* ct : constraints_) {
    if (ct->type == "false_constraint") {
      return true;
    }
  }

  return false;
}

// ----- Model statistics -----

void ModelStatistics::PrintStatistics() const {
  SOLVER_LOG(logger_, "Model ", model_.name());
  for (const auto& it : constraints_per_type_) {
    SOLVER_LOG(logger_, "  - ", it.first, ": ", it.second.size());
  }
  if (model_.objective() == nullptr) {
    SOLVER_LOG(logger_, "  - Satisfaction problem");
  } else {
    SOLVER_LOG(logger_, "  - ",
               (model_.maximize() ? "Maximization" : "Minimization"),
               " problem");
  }
  SOLVER_LOG(logger_);
}

void ModelStatistics::BuildStatistics() {
  constraints_per_type_.clear();
  constraints_per_variables_.clear();
  for (Constraint* const ct : model_.constraints()) {
    if (ct != nullptr && ct->active) {
      constraints_per_type_[ct->type].push_back(ct);
      absl::flat_hash_set<const Variable*> marked;
      for (const Argument& arg : ct->arguments) {
        for (Variable* const var : arg.variables) {
          marked.insert(var);
        }
      }
      for (const Variable* const var : marked) {
        constraints_per_variables_[var].push_back(ct);
      }
    }
  }
}

// Flatten Search annotations.
void FlattenAnnotations(const Annotation& ann, std::vector<Annotation>* out) {
  if (ann.type == Annotation::ANNOTATION_LIST ||
      ann.IsFunctionCallWithIdentifier("seq_search")) {
    for (const Annotation& inner : ann.annotations) {
      FlattenAnnotations(inner, out);
    }
  } else {
    out->push_back(ann);
  }
}

}  // namespace fz
}  // namespace operations_research
