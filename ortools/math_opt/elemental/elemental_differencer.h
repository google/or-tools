// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_MATH_OPT_ELEMENTAL_ELEMENTAL_DIFFERENCER_H_
#define ORTOOLS_MATH_OPT_ELEMENTAL_ELEMENTAL_DIFFERENCER_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"

namespace operations_research::math_opt {

// Returns the elements in both first and second.
template <typename T>
absl::flat_hash_set<T> IntersectSets(const absl::flat_hash_set<T>& first,
                                     const absl::flat_hash_set<T>& second);

// The elements in the set first, but not in second, and the elements in the set
// second, but not in first.
template <typename T>
struct SymmetricDifference {
  absl::flat_hash_set<T> only_in_first;
  absl::flat_hash_set<T> only_in_second;

  explicit SymmetricDifference() = default;
  explicit SymmetricDifference(const absl::flat_hash_set<T>& first,
                               const absl::flat_hash_set<T>& second);

  bool Empty() const { return only_in_first.empty() && only_in_second.empty(); }
};

// TODO(b/368421402): many features are missing here, e.g. floating point
// tolerance, allowing variables to be permuted.
struct ElementalDifferenceOptions {
  bool check_names = true;
  bool check_next_id = true;
};

// Holds the difference between the model from two `Elemental`s. Typically used
// to check if two Elemental objects are the same, and provide a nice
// description of how they differ if they are not the same.
//
// Note that:
//  * This is an entirely separate concept from the Diff object held by an
//    an Elemental, which tracks changes to an Elemental from a point in time.
//    The similarity in names is unfortunate and confusing.
//  * This class only tracks differences between the models, it ignores any Diff
//    objects either Elemental contains.
class ElementalDifference {
 public:
  // The difference for an `ElementType`.
  struct ElementDifference {
    // Element ids in one elemental but not in the other.
    SymmetricDifference<int64_t> ids;

    // Element ids in both elementals where the element names disagree.
    absl::flat_hash_set<int64_t> different_names;

    // The value of next_id for this ElementType differs.
    bool next_id_different = false;

    // Indicates there are no differences for this ElementType.
    bool Empty() const;
  };

  // The difference for an attribute. E.g. for the attribute
  // DoubleAttr1::kVarLb, we would take `AttrType` DoubleAttr1.
  template <typename AttrType>
  struct AttributeDifference {
    using Key = AttrKeyFor<AttrType>;

    // The keys with non-default value for this attribute in one Elemental but
    // not the other.
    SymmetricDifference<Key> keys;

    // The keys where the attribute has a different non-default value in each
    // Elemental.
    absl::flat_hash_set<Key> different_values;

    // Returns every key in `keys` or `different_values` (the keys that the
    // attribute is different on).
    std::vector<Key> AllKeysSorted() const;

    // Indicates that there are no differences for this attribute.
    bool Empty() const;
  };

  // Returns the difference between two Elementals.
  static ElementalDifference Create(
      const Elemental& first, const Elemental& second,
      const ElementalDifferenceOptions& options = {});
  explicit ElementalDifference() = default;

  // Returns a string describing the difference between two models.
  static std::string DescribeDifference(
      const Elemental& first, const Elemental& second,
      const ElementalDifferenceOptions& options = {});

  // Returns a string describing `difference`, using data from `first` and
  // `second` to make the output more human readable (e.g. show element names),
  // or CHECK fails on bad input (see below).
  //
  // Advanced use, generally prefer `DescribeDifference(const Elemental&,
  // const Elemental&, const ElementalDifferenceOptions&)` instead, which cannot
  // CHECK fail. Useful if you want to compute the difference and do some
  // logical operations before (perhaps conditionally) converting the difference
  // to a string.
  //
  // This function can CHECK fail if `difference` claims that `first` or
  // `second` contains an element which is missing. A sufficient condition to
  // ensure that this function will NOT CHECK is to invoke it as
  //   Describe(first, second, Create(first, second))
  // to ensure that the models are correct and not modified between Create() and
  // Describe().
  static std::string Describe(const Elemental& first, const Elemental& second,
                              const ElementalDifference& difference);

  inline const ElementDifference& element_difference(
      const ElementType e) const {
    return elements_[static_cast<int>(e)];
  }

  inline ElementDifference& mutable_element_difference(const ElementType e) {
    return elements_[static_cast<int>(e)];
  }

  template <typename AttrType>
  const AttributeDifference<AttrType>& attr_difference(const AttrType a) const {
    return attrs_[a];
  }

  template <typename AttrType>
  AttributeDifference<AttrType>& mutable_attr_difference(const AttrType a) {
    return attrs_[a];
  }

  bool Empty() const;

  bool model_name_different() const { return model_name_different_; }
  inline void set_model_name_different(bool value) {
    model_name_different_ = value;
  }

  bool primary_objective_name_different() const {
    return primary_objective_name_different_;
  }
  inline void set_primary_objective_name_different(bool value) {
    primary_objective_name_different_ = value;
  }

 private:
  bool model_name_different_ = false;
  bool primary_objective_name_different_ = false;
  std::array<ElementDifference, kNumElements> elements_;
  template <int i>
  using StorageForAttr = AttributeDifference<AllAttrs::Type<i>>;
  AttrMap<StorageForAttr> attrs_;
};

////////////////////////////////////////////////////////////////////////////////
// Template function implementations
////////////////////////////////////////////////////////////////////////////////

template <typename T>
absl::flat_hash_set<T> IntersectSets(const absl::flat_hash_set<T>& first,
                                     const absl::flat_hash_set<T>& second) {
  absl::flat_hash_set<T> result;
  for (const T& f : first) {
    if (second.contains(f)) {
      result.insert(f);
    }
  }
  return result;
}

template <typename T>
SymmetricDifference<T>::SymmetricDifference(
    const absl::flat_hash_set<T>& first, const absl::flat_hash_set<T>& second) {
  for (const T& f : first) {
    if (!second.contains(f)) {
      only_in_first.insert(f);
    }
  }
  for (const T& s : second) {
    if (!first.contains(s)) {
      only_in_second.insert(s);
    }
  }
}

template <typename AttrType>
std::vector<typename ElementalDifference::AttributeDifference<AttrType>::Key>
ElementalDifference::AttributeDifference<AttrType>::AllKeysSorted() const {
  std::vector<Key> result;
  for (Key key : keys.only_in_first) {
    result.push_back(key);
  }
  for (Key key : keys.only_in_second) {
    result.push_back(key);
  }
  for (Key key : different_values) {
    result.push_back(key);
  }
  absl::c_sort(result);
  return result;
}

template <typename AttrType>
bool ElementalDifference::AttributeDifference<AttrType>::Empty() const {
  return keys.Empty() && different_values.empty();
}

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_ELEMENTAL_ELEMENTAL_DIFFERENCER_H_
