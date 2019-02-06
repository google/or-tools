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

#ifndef OR_TOOLS_UTIL_AFFINE_RELATION_H_
#define OR_TOOLS_UTIL_AFFINE_RELATION_H_

#include <vector>

#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

namespace operations_research {

// Union-Find algorithm to maintain "representative" for relations of the form:
// x = coeff * y + offset, where "coeff" and "offset" are integers. The variable
// x and y are represented by non-negative integer indices. The idea is to
// express variables in affine relation using as little different variables as
// possible (the representatives).
//
// IMPORTANT: If there are relations with std::abs(coeff) != 1, then some
// relations might be ignored. See TryAdd() for more details.
//
// TODO(user): it might be possible to do something fancier and drop less
// relations if all the affine relations are given before hand.
class AffineRelation {
 public:
  AffineRelation() : num_relations_(0) {}

  // Returns the number of relations added to the class and not ignored.
  int NumRelations() const { return num_relations_; }

  // Adds the relation x = coeff * y + offset to the class.
  // Returns true if it wasn't ignored.
  //
  // This relation will only be taken into account if the representative of x
  // and the one of y are different and if the relation can be transformed into
  // a similar relation with integer coefficient between the two
  // representatives.
  //
  // That is, given that:
  // - x = coeff_x * representative_x + offset_x
  // - y = coeff_y * representative_y + offset_y
  // we have:
  //   coeff_x * representative_x + offset_x =
  //       coeff * coeff_y * representative_y + coeff * offset_y + offset.
  // Which can be simplified with the introduction of new variables to:
  //   coeff_x * representative_x = new_coeff * representative_y + new_offset.
  // And we can merge the two if:
  //  - new_coeff and new_offset are divisible by coeff_x.
  //  - OR coeff_x and new_offset are divisible by new_coeff.
  //
  // Checked preconditions: x >=0, y >= 0, coeff != 0 and x != y.
  //
  // IMPORTANT: we do not check for integer overflow, but that could be added
  // if it is needed.
  bool TryAdd(int x, int y, int64 coeff, int64 offset);

  // Same as TryAdd() with the option to disallow the use of a given
  // representative.
  bool TryAdd(int x, int y, int64 coeff, int64 offset, bool allow_rep_x,
              bool allow_rep_y);

  // Returns a valid relation of the form x = coeff * representative + offset.
  // Note that this can return x = x. Non-const because of path-compression.
  struct Relation {
    int representative;
    int64 coeff;
    int64 offset;
    Relation(int r, int64 c, int64 o)
        : representative(r), coeff(c), offset(o) {}
    const bool operator==(const Relation& other) const {
      return representative == other.representative && coeff == other.coeff &&
             offset == other.offset;
    }
  };
  Relation Get(int x) const;

  // Returns the size of the class of x.
  int ClassSize(int x) const {
    if (x >= representative_.size()) return 1;
    return size_[Get(x).representative];
  }

 private:
  void IncreaseSizeOfMemberVectors(int new_size) {
    if (new_size <= representative_.size()) return;
    for (int i = representative_.size(); i < new_size; ++i) {
      representative_.push_back(i);
    }
    offset_.resize(new_size, 0);
    coeff_.resize(new_size, 1);
    size_.resize(new_size, 1);
  }

  void CompressPath(int x) const {
    DCHECK_GE(x, 0);
    DCHECK_LT(x, representative_.size());
    tmp_path_.clear();
    int parent = x;
    while (parent != representative_[parent]) {
      tmp_path_.push_back(parent);
      parent = representative_[parent];
    }
    for (const int var : ::gtl::reversed_view(tmp_path_)) {
      const int old_parent = representative_[var];
      offset_[var] += coeff_[var] * offset_[old_parent];
      coeff_[var] *= coeff_[old_parent];
      representative_[var] = parent;
    }
  }

  int num_relations_;

  // The equivalence class representative for each variable index.
  mutable std::vector<int> representative_;

  // The offset and coefficient such that
  // variable[index] = coeff * variable[representative_[index]] + offset;
  mutable std::vector<int64> coeff_;
  mutable std::vector<int64> offset_;

  // The size of each representative "tree", used to get a good complexity when
  // we have the choice of which tree to merge into the other.
  //
  // TODO(user): Using a "rank" might be faster, but because we sometimes
  // need to merge the bad subtree into the better one, it is trickier to
  // maintain than in the classic union-find algorihtm.
  std::vector<int> size_;

  // Used by CompressPath() to maintain the coeff/offset during compression.
  mutable std::vector<int> tmp_path_;
};

inline bool AffineRelation::TryAdd(int x, int y, int64 coeff, int64 offset) {
  return TryAdd(x, y, coeff, offset, true, true);
}

inline bool AffineRelation::TryAdd(int x, int y, int64 coeff, int64 offset,
                                   bool allow_rep_x, bool allow_rep_y) {
  CHECK_NE(coeff, 0);
  CHECK_NE(x, y);
  CHECK_GE(x, 0);
  CHECK_GE(y, 0);
  IncreaseSizeOfMemberVectors(std::max(x, y) + 1);
  CompressPath(x);
  CompressPath(y);
  const int rep_x = representative_[x];
  const int rep_y = representative_[y];
  if (rep_x == rep_y) return false;

  // TODO(user): It should be possible to optimize this code block a bit, for
  // instance depending on the magnitude of new_coeff vs coeff_x, we may already
  // know that one of the two merge is not possible.
  const int64 coeff_x = coeff_[x];
  const int64 new_coeff = coeff * coeff_[y];
  const int64 new_offset = coeff * offset_[y] + offset - offset_[x];
  const bool condition1 =
      allow_rep_y && (new_coeff % coeff_x == 0) && (new_offset % coeff_x == 0);
  const bool condition2 = allow_rep_x && (coeff_x % new_coeff == 0) &&
                          (new_offset % new_coeff == 0);
  if (condition1 && (!condition2 || size_[x] <= size_[y])) {
    representative_[rep_x] = rep_y;
    size_[rep_y] += size_[rep_x];
    coeff_[rep_x] = new_coeff / coeff_x;
    offset_[rep_x] = new_offset / coeff_x;
  } else if (condition2) {
    representative_[rep_y] = rep_x;
    size_[rep_x] += size_[rep_y];
    coeff_[rep_y] = coeff_x / new_coeff;
    offset_[rep_y] = -new_offset / new_coeff;
  } else {
    return false;
  }
  ++num_relations_;
  return true;
}

inline AffineRelation::Relation AffineRelation::Get(int x) const {
  if (x >= representative_.size() || representative_[x] == x) return {x, 1, 0};
  CompressPath(x);
  return {representative_[x], coeff_[x], offset_[x]};
}

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_AFFINE_RELATION_H_
