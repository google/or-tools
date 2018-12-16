// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_SAT_CP_MODEL_UTILS_H_
#define OR_TOOLS_SAT_CP_MODEL_UTILS_H_

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/bitset.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

// Small utility functions to deal with negative variable/literal references.
inline int NegatedRef(int ref) { return -ref - 1; }
inline int PositiveRef(int ref) { return std::max(ref, NegatedRef(ref)); }
inline bool RefIsPositive(int ref) { return ref >= 0; }

// Small utility functions to deal with half-reified constraints.
inline bool HasEnforcementLiteral(const ConstraintProto& ct) {
  return !ct.enforcement_literal().empty();
}
inline int EnforcementLiteral(const ConstraintProto& ct) {
  return ct.enforcement_literal(0);
}

// Collects all the references used by a constraint. This function is used in a
// few places to have a "generic" code dealing with constraints. Note that the
// enforcement_literal is NOT counted here.
//
// TODO(user): replace this by constant version of the Apply...() functions?
struct IndexReferences {
  absl::flat_hash_set<int> variables;
  absl::flat_hash_set<int> literals;
  absl::flat_hash_set<int> intervals;
};
void AddReferencesUsedByConstraint(const ConstraintProto& ct,
                                   IndexReferences* output);

// Applies the given function to all variables/literals/intervals indices of the
// constraint. This function is used in a few places to have a "generic" code
// dealing with constraints.
void ApplyToAllVariableIndices(const std::function<void(int*)>& function,
                               ConstraintProto* ct);
void ApplyToAllLiteralIndices(const std::function<void(int*)>& function,
                              ConstraintProto* ct);
void ApplyToAllIntervalIndices(const std::function<void(int*)>& function,
                               ConstraintProto* ct);

// Returns the name of the ConstraintProto::ConstraintCase oneof enum.
// Note(user): There is no such function in the proto API as of 16/01/2017.
std::string ConstraintCaseName(ConstraintProto::ConstraintCase constraint_case);

// Returns the sorted list of variables used by a constraint.
std::vector<int> UsedVariables(const ConstraintProto& ct);

// Returns true if a proto.domain() contain the given value.
// The domain is expected to be encoded as a sorted disjoint interval list.
template <typename ProtoWithDomain>
bool DomainInProtoContains(const ProtoWithDomain& proto, int64 value) {
  for (int i = 0; i < proto.domain_size(); i += 2) {
    if (value >= proto.domain(i) && value <= proto.domain(i + 1)) return true;
  }
  return false;
}

// Serializes a Domain into the domain field of a proto.
template <typename ProtoWithDomain>
void FillDomainInProto(const Domain& domain, ProtoWithDomain* proto) {
  proto->clear_domain();
  for (const ClosedInterval& interval : domain) {
    proto->add_domain(interval.start);
    proto->add_domain(interval.end);
  }
}

// Reads a Domain from the domain field of a proto.
template <typename ProtoWithDomain>
Domain ReadDomainFromProto(const ProtoWithDomain& proto) {
  std::vector<ClosedInterval> intervals;
  for (int i = 0; i < proto.domain_size(); i += 2) {
    intervals.push_back({proto.domain(i), proto.domain(i + 1)});
  }
  return Domain::FromIntervals(intervals);
}

// Returns the list of values in a given domain.
// This will fail if the domain contains more than one millions values.
//
// TODO(user): work directly on the Domain class instead.
template <typename ProtoWithDomain>
std::vector<int64> AllValuesInDomain(const ProtoWithDomain& proto) {
  std::vector<int64> result;
  for (int i = 0; i < proto.domain_size(); i += 2) {
    for (int64 v = proto.domain(i); v <= proto.domain(i + 1); ++v) {
      CHECK_LE(result.size(), 1e6);
      result.push_back(v);
    }
  }
  return result;
}

class SharedBoundsManager {
 public:
  SharedBoundsManager(int num_workers, int num_variables)
      : num_workers_(num_workers),
        num_variables_(num_variables),
        changed_variables_per_workers_(num_workers),
        lower_bounds_(num_variables, kint64min),
        upper_bounds_(num_variables, kint64max) {
    for (int i = 0; i < num_workers_; ++i) {
      changed_variables_per_workers_[i].ClearAndResize(num_variables_);
    }
  }

  void ReportPotentialNewBounds(const std::vector<int>& variables,
                                const std::vector<int64>& new_lower_bounds,
                                const std::vector<int64>& new_upper_bounds,
                                int worker_id, const std::string& worker_name) {
    CHECK_EQ(variables.size(), new_lower_bounds.size());
    CHECK_EQ(variables.size(), new_upper_bounds.size());
    {
      absl::MutexLock mutex_lock(&mutex_);
      int modified_domains = 0;
      int fixed_domains = 0;
      for (int i = 0; i < variables.size(); ++i) {
        const int var = variables[i];
        if (var >= num_variables_) continue;
        const int64 new_lb = new_lower_bounds[i];
        const int64 new_ub = new_upper_bounds[i];
        CHECK_GE(var, 0);
        bool changed = false;
        if (lower_bounds_[var] < new_lb) {
          changed = true;
          lower_bounds_[var] = new_lb;
        }
        if (upper_bounds_[var] > new_ub) {
          changed = true;
          upper_bounds_[var] = new_ub;
        }
        if (changed) {
          if (lower_bounds_[var] == upper_bounds_[var]) {
            fixed_domains++;
          } else {
            modified_domains++;
          }
          for (int j = 0; j < num_workers_; ++j) {
            if (worker_id == j) continue;
            changed_variables_per_workers_[j].Set(var);
          }
        }
      }
      if (fixed_domains > 0 || modified_domains > 0) {
        VLOG(1) << "Worker " << worker_name
                << ": fixed domains=" << fixed_domains
                << ", modified domains=" << modified_domains << " out of "
                << variables.size() << " events";
      }
    }
  }

  void GetChangedBounds(int worker_id, std::vector<int>* variables,
                        std::vector<int64>* new_lower_bounds,
                        std::vector<int64>* new_upper_bounds) {
    variables->clear();
    new_lower_bounds->clear();
    new_upper_bounds->clear();
    {
      absl::MutexLock mutex_lock(&mutex_);
      for (const int var : changed_variables_per_workers_[worker_id]
                               .PositionsSetAtLeastOnce()) {
        variables->push_back(var);
        new_lower_bounds->push_back(lower_bounds_[var]);
        new_upper_bounds->push_back(upper_bounds_[var]);
      }
      changed_variables_per_workers_[worker_id].ClearAll();
    }
  }

 private:
  const int num_workers_;
  const int num_variables_;
  std::vector<SparseBitset<int64>> changed_variables_per_workers_;
  std::vector<int64> lower_bounds_;
  std::vector<int64> upper_bounds_;
  absl::Mutex mutex_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_UTILS_H_
