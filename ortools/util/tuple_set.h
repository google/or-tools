// Copyright 2010-2017 Google
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


// Set of integer tuples (fixed-size arrays, all of the same size) with
// a basic API.
// It supports several types of integer arrays transparently, with an
// inherent storage based on int64 arrays.
//
// The key feature is the "lazy" copy:
// - Copying an IntTupleSet won't actually copy the data right away; we
//   will just have several IntTupleSet pointing at the same data.
// - Modifying an IntTupleSet which shares his data with others
//   will create a new, modified instance of the data payload, and make
//   the IntTupleSet point to that new data.
// - Modifying an IntTupleSet that doesn't share its data with any other
//   IntTupleSet will modify the data directly.
// Therefore, you don't need to use const IntTupleSet& in methods. Just do:
// void MyMethod(IntTupleSet tuple_set) { ... }
//
// This class is thread hostile as the copy and reference counter are
// not protected by a mutex.

#ifndef OR_TOOLS_UTIL_TUPLE_SET_H_
#define OR_TOOLS_UTIL_TUPLE_SET_H_

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/map_util.h"
#include "ortools/base/hash.h"

namespace operations_research {
// ----- Main IntTupleSet class -----
class IntTupleSet {
 public:
  // Creates an empty tuple set with a fixed length for all tuples.
  explicit IntTupleSet(int arity);
  // Copy constructor (it actually does a lazy copy, see toplevel comment).
  IntTupleSet(const IntTupleSet& set);  // NOLINT
  ~IntTupleSet();

  // Clears data.
  void Clear();

  // Inserts the tuple to the set. It does nothing if the tuple is
  // already in the set. The size of the tuple must be equal to the
  // arity of the set. It returns the index at which the tuple was
  // inserted (-1 if it was already present).
  int Insert(const std::vector<int>& tuple);
  int Insert(const std::vector<int64>& tuple);
  // Arity fixed version of Insert removing the need for a vector for the user.
  int Insert2(int64 v0, int64 v1);
  int Insert3(int64 v0, int64 v1, int64 v2);
  int Insert4(int64 v0, int64 v1, int64 v2, int64 v3);
  // Inserts the tuples.
  void InsertAll(const std::vector<std::vector<int64> >& tuples);
  void InsertAll(const std::vector<std::vector<int> >& tuples);

  // Checks if the tuple is in the set.
  bool Contains(const std::vector<int>& tuple) const;
  bool Contains(const std::vector<int64>& tuple) const;

  // Returns the number of tuples.
  int NumTuples() const;
  // Get the given tuple's value at the given position.  The indices
  // of the tuples correspond to the order in which they were
  // inserted.
  int64 Value(int tuple_index, int pos_in_tuple) const;
  // Returns the arity of the set.
  int Arity() const;
  // Access the raw data, see IntTupleSet::Data::flat_tuples_.
  const int64* RawData() const;
  // Returns the number of different values in the given column.
  int NumDifferentValuesInColumn(int col) const;
  // Return a copy of the set, sorted by the "col"-th value of each
  // tuples. The sort is stable.
  IntTupleSet SortedByColumn(int col) const;
  // Returns a copy of the tuple set lexicographically sorted.
  IntTupleSet SortedLexicographically() const;

 private:
  // Class that holds the actual data of an IntTupleSet. It handles
  // the reference counters, etc.
  class Data {
   public:
    explicit Data(int arity);
    Data(const Data& data);
    ~Data();
    void AddSharedOwner();
    bool RemovedSharedOwner();
    Data* CopyIfShared();
    template <class T>
    int Insert(const std::vector<T>& tuple);
    template <class T>
    bool Contains(const std::vector<T>& candidate) const;
    template <class T>
    int64 Fingerprint(const std::vector<T>& tuple) const;
    int NumTuples() const;
    int64 Value(int index, int pos) const;
    int Arity() const;
    const int64* RawData() const;
    void Clear();

   private:
    const int arity_;
    int num_owners_;
    // Concatenation of all tuples ever added.
    std::vector<int64> flat_tuples_;
    // Maps a tuple's fingerprint to the list of tuples with this
    // fingerprint, represented by their start index in the
    // flat_tuples_ vector.
    std::unordered_map<int64, std::vector<int> > tuple_fprint_to_index_;
  };

  // Used to represent a light representation of a tuple.
  struct IndexData {
    int index;
    IntTupleSet::Data* data;
    IndexData(int i, IntTupleSet::Data* const d) : index(i), data(d) {}
    static bool Compare(const IndexData& tuple1, const IndexData& tuple2);
  };

  struct IndexValue {
    int index;
    int64 value;
    IndexValue(int i, int64 v) : index(i), value(v) {}
    static bool Compare(const IndexValue& tuple1, const IndexValue& tuple2);
  };

  mutable Data* data_;
};

// ----- Data -----
inline IntTupleSet::Data::Data(int arity) : arity_(arity), num_owners_(0) {}

inline IntTupleSet::Data::Data(const Data& data)
    : arity_(data.arity_),
      num_owners_(0),
      flat_tuples_(data.flat_tuples_),
      tuple_fprint_to_index_(data.tuple_fprint_to_index_) {}

inline IntTupleSet::Data::~Data() {}

inline void IntTupleSet::Data::AddSharedOwner() { num_owners_++; }

inline bool IntTupleSet::Data::RemovedSharedOwner() {
  return (--num_owners_ == 0);
}

inline IntTupleSet::Data* IntTupleSet::Data::CopyIfShared() {
  if (num_owners_ > 1) {  // Copy on write.
    Data* const new_data = new Data(*this);
    RemovedSharedOwner();
    new_data->AddSharedOwner();
    return new_data;
  }
  return this;
}

template <class T>
int IntTupleSet::Data::Insert(const std::vector<T>& tuple) {
  DCHECK(arity_ == 0 || flat_tuples_.size() % arity_ == 0);
  CHECK_EQ(arity_, tuple.size());
  DCHECK_EQ(1, num_owners_);
  if (!Contains(tuple)) {
    const int index = NumTuples();
    const int offset = flat_tuples_.size();
    flat_tuples_.resize(offset + arity_);
    // On mac os X, using this instead of push_back gives a 10x speedup!
    for (int i = 0; i < arity_; ++i) {
      flat_tuples_[offset + i] = tuple[i];
    }
    const int64 fingerprint = Fingerprint(tuple);
    tuple_fprint_to_index_[fingerprint].push_back(index);
    return index;
  } else {
    return -1;
  }
}

template <class T>
bool IntTupleSet::Data::Contains(const std::vector<T>& candidate) const {
  if (candidate.size() != arity_) {
    return false;
  }
  const int64 fingerprint = Fingerprint(candidate);
  if (ContainsKey(tuple_fprint_to_index_, fingerprint)) {
    const std::vector<int>& indices =
        FindOrDie(tuple_fprint_to_index_, fingerprint);
    for (int i = 0; i < indices.size(); ++i) {
      const int tuple_index = indices[i];
      for (int j = 0; j < arity_; ++j) {
        if (candidate[j] != flat_tuples_[tuple_index * arity_ + j]) {
          return false;
        }
      }
      return true;
    }
  }
  return false;
}

template <class T>
int64 IntTupleSet::Data::Fingerprint(const std::vector<T>& tuple) const {
  switch (arity_) {
    case 0:
      return 0;
    case 1:
      return tuple[0];
    case 2: {
      uint64 x = tuple[0];
      uint64 y = GG_ULONGLONG(0xe08c1d668b756f82);
      uint64 z = tuple[1];
      mix(x, y, z);
      return z;
    }
    default: {
      uint64 x = tuple[0];
      uint64 y = GG_ULONGLONG(0xe08c1d668b756f82);
      for (int i = 1; i < tuple.size(); ++i) {
        uint64 z = tuple[i];
        mix(x, y, z);
        x = z;
      }
      return x;
    }
  }
}

inline int IntTupleSet::Data::NumTuples() const {
  return tuple_fprint_to_index_.size();
}

inline int64 IntTupleSet::Data::Value(int index, int pos) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, flat_tuples_.size() / arity_);
  DCHECK_GE(pos, 0);
  DCHECK_LT(pos, arity_);
  return flat_tuples_[index * arity_ + pos];
}

inline int IntTupleSet::Data::Arity() const { return arity_; }

inline const int64* IntTupleSet::Data::RawData() const {
  return flat_tuples_.data();
}

inline void IntTupleSet::Data::Clear() {
  flat_tuples_.clear();
  tuple_fprint_to_index_.clear();
}

inline IntTupleSet::IntTupleSet(int arity) : data_(new Data(arity)) {
  CHECK_GE(arity, 0);
  data_->AddSharedOwner();
}

inline IntTupleSet::IntTupleSet(const IntTupleSet& set) : data_(set.data_) {
  data_->AddSharedOwner();
}

inline IntTupleSet::~IntTupleSet() {
  CHECK_NOTNULL(data_);
  if (data_->RemovedSharedOwner()) {
    delete data_;
  }
}

inline void IntTupleSet::Clear() {
  data_ = data_->CopyIfShared();
  data_->Clear();
}

inline int IntTupleSet::Insert(const std::vector<int>& tuple) {
  data_ = data_->CopyIfShared();
  return data_->Insert(tuple);
}

inline int IntTupleSet::Insert(const std::vector<int64>& tuple) {
  data_ = data_->CopyIfShared();
  return data_->Insert(tuple);
}

inline int IntTupleSet::Insert2(int64 v0, int64 v1) {
  std::vector<int64> tuple(2);
  tuple[0] = v0;
  tuple[1] = v1;
  return Insert(tuple);
}

inline int IntTupleSet::Insert3(int64 v0, int64 v1, int64 v2) {
  std::vector<int64> tuple(3);
  tuple[0] = v0;
  tuple[1] = v1;
  tuple[2] = v2;
  return Insert(tuple);
}

inline int IntTupleSet::Insert4(int64 v0, int64 v1, int64 v2, int64 v3) {
  std::vector<int64> tuple(4);
  tuple[0] = v0;
  tuple[1] = v1;
  tuple[2] = v2;
  tuple[3] = v3;
  return Insert(tuple);
}

inline bool IntTupleSet::Contains(const std::vector<int>& tuple) const {
  return data_->Contains(tuple);
}

inline bool IntTupleSet::Contains(const std::vector<int64>& tuple) const {
  return data_->Contains(tuple);
}

inline void IntTupleSet::InsertAll(
    const std::vector<std::vector<int> >& tuples) {
  data_ = data_->CopyIfShared();
  for (int i = 0; i < tuples.size(); ++i) {
    Insert(tuples[i]);
  }
}

inline void IntTupleSet::InsertAll(
    const std::vector<std::vector<int64> >& tuples) {
  data_ = data_->CopyIfShared();
  for (int i = 0; i < tuples.size(); ++i) {
    Insert(tuples[i]);
  }
}

inline int IntTupleSet::NumTuples() const { return data_->NumTuples(); }

inline int64 IntTupleSet::Value(int index, int pos) const {
  return data_->Value(index, pos);
}

inline int IntTupleSet::Arity() const { return data_->Arity(); }

inline const int64* IntTupleSet::RawData() const { return data_->RawData(); }

inline int IntTupleSet::NumDifferentValuesInColumn(int col) const {
  if (col < 0 || col >= data_->Arity()) {
    return 0;
  }
  std::unordered_set<int64> values;
  for (int i = 0; i < data_->NumTuples(); ++i) {
    values.insert(data_->Value(i, col));
  }
  return values.size();
}

inline bool IntTupleSet::IndexValue::Compare(const IndexValue& a,
                                             const IndexValue& b) {
  return a.value < b.value || (a.value == b.value && a.index < b.index);
}

inline IntTupleSet IntTupleSet::SortedByColumn(int col) const {
  std::vector<IndexValue> keys;
  keys.reserve(data_->NumTuples());
  for (int index = 0; index < data_->NumTuples(); ++index) {
    keys.push_back(IndexValue(index, data_->Value(index, col)));
  }
  std::sort(keys.begin(), keys.end(), IntTupleSet::IndexValue::Compare);
  const int arity = data_->Arity();
  IntTupleSet sorted(arity);
  for (int i = 0; i < keys.size(); ++i) {
    const int64* tuple_ptr = data_->RawData() + keys[i].index * arity;
    sorted.Insert(std::vector<int64>(tuple_ptr, tuple_ptr + arity));
  }
  return sorted;
}

inline bool IntTupleSet::IndexData::Compare(const IndexData& a,
                                            const IndexData& b) {
  const IntTupleSet::Data* const data = a.data;
  const int arity = data->Arity();
  for (int i = 0; i < arity; ++i) {
    const int64 value1 = data->Value(a.index, i);
    const int64 value2 = data->Value(b.index, i);
    if (value1 < value2) {
      return true;
    }
    if (value1 > value2) {
      return false;
    }
  }
  return false;
}

inline IntTupleSet IntTupleSet::SortedLexicographically() const {
  std::vector<IndexData> keys;
  keys.reserve(data_->NumTuples());
  for (int index = 0; index < data_->NumTuples(); ++index) {
    keys.push_back(IndexData(index, data_));
  }
  std::sort(keys.begin(), keys.end(), IntTupleSet::IndexData::Compare);
  const int arity = data_->Arity();
  IntTupleSet sorted(arity);
  for (int i = 0; i < keys.size(); ++i) {
    std::vector<int64> tuple(arity);
    const int64* tuple_ptr = data_->RawData() + keys[i].index * arity;
    sorted.Insert(std::vector<int64>(tuple_ptr, tuple_ptr + arity));
  }
  return sorted;
}
}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_TUPLE_SET_H_
