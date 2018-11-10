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

//
// Classes for permuting indexable, ordered containers of data without
// depending on that data to be accessible in any particular way. The
// client needs to give us two things:
//   1. a permutation to apply to some container(s) of data, and
//   2. a description of how to move data around in the container(s).
//
// The permutation (1) comes to us in the form of an array argument to
// PermutationApplier::Apply(), along with index values that tell us
// where in that array the permutation of interest lies. Typically
// those index values will span the entire array that describes the
// permutation.
//
// Applying a permutation involves decomposing the permutation into
// disjoint cycles and walking each element of the underlying data one
// step around the unique cycle in which it participates. The
// decomposition into disjoint cycles is done implicitly on the fly as
// the code in PermutationApplier::Apply() advances through the array
// describing the permutation. As an important piece of bookkeeping to
// support the decomposition into cycles, the elements of the
// permutation array typically get modified somehow to indicate which
// ones have already been used.
//
// At first glance, it would seem that if the containers are
// indexable, we don't need anything more complicated than just the
// permutation and the container of data we want to permute; it would
// seem we can just use the container's operator[] to retrieve and
// assign elements within the container. Unfortunately it's not so
// simple because the containers of interest can be indexable without
// providing any consistent way of accessing their contents that
// applies to all the containers of interest. For instance, if we
// could insist that every indexable container must define an lvalue
// operator[]() we could simply use that for the assignments we need
// to do while walking around cycles of the permutation. But we cannot
// insist on any such thing. To see why, consider the PackedArray
// class template in ortools/util/packed_array.h
// where operator[] is supplied for rvalues, but because each logical
// array element is packed across potentially multiple instances of
// the underlying data type that the C++ language knows about, there
// is no way to have a C++ reference to an element of a
// PackedArray. There are other such examples besides PackedArray,
// too. This is the main reason we need a codified description (2) of
// how to move data around in the indexable container. That
// description comes to us via the PermutationApplier constructor's
// argument which is a PermutationCycleHandler instance. Such an
// object has three important methods defined: SetTempFromIndex(),
// SetIndexFromIndex(), and SetIndexFromTemp(). Those methods embody
// all we need to know about how to move data in the indexable
// container(s) underlying the PermutationCycleHandler.
//
// Another reason we need the description (2) of how to move elements
// around in the container(s) is that it is often important to permute
// side-by-side containers of elements according to the same
// permutation. This situation, too, is covered by defining a
// PermutationCycleHandler that knows about multiple underlying
// indexable containers.
//
// The above-mentioned PermutationCycleHandler methods embody
// knowledge of how to assign elements. It happens that
// PermutationCycleHandler is also a convenient place to embody the
// knowledge of how to keep track of which permutation elements have
// been consumed by the process of walking data around cycles. We
// depend on the PermutationCycleHandler instance we're given to
// define SetSeen() and Unseen() methods for that purpose.
//
// For the common case in which elements can be accessed using
// operator[](), we provide the class template
// ArrayIndexCycleHandler.

#ifndef OR_TOOLS_UTIL_PERMUTATION_H_
#define OR_TOOLS_UTIL_PERMUTATION_H_

#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

namespace operations_research {

// Abstract base class template defining the interface needed by
// PermutationApplier to handle a single cycle of a permutation.
template <typename IndexType>
class PermutationCycleHandler {
 public:
  // Sets the internal temporary storage from the given index in the
  // underlying container(s).
  virtual void SetTempFromIndex(IndexType source) = 0;

  // Moves a data element one step along its cycle.
  virtual void SetIndexFromIndex(IndexType source,
                                 IndexType destination) const = 0;

  // Sets a data element from the temporary.
  virtual void SetIndexFromTemp(IndexType destination) const = 0;

  // Marks an element of the permutation as handled by
  // PermutationHandler::Apply(), meaning that we have read the
  // corresponding value from the data to be permuted, and put that
  // value somewhere (either in the temp or in its ultimate
  // destination in the data.
  //
  // This method must be overridden in implementations where it is
  // called. If an implementation doesn't call it, no need to
  // override.
  virtual void SetSeen(IndexType* unused_permutation_element) const {
    LOG(FATAL) << "Base implementation of SetSeen() must not be called.";
  }

  // Returns true iff the given element of the permutation is unseen,
  // meaning that it has not yet been handled by
  // PermutationApplier::Apply().
  //
  // This method must be overridden in implementations where it is
  // called. If an implementation doesn't call it, no need to
  // override.
  virtual bool Unseen(IndexType unused_permutation_element) const {
    LOG(FATAL) << "Base implementation of Unseen() must not be called.";
    return false;
  }

  virtual ~PermutationCycleHandler() {}

 protected:
  PermutationCycleHandler() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PermutationCycleHandler);
};

// A generic cycle handler class for the common case in which the
// object to be permuted is indexable with T& operator[](int), and the
// permutation is represented by a mutable array of nonnegative
// int-typed index values. To mark a permutation element as seen, we
// replace it by its ones-complement value.
template <typename DataType, typename IndexType>
class ArrayIndexCycleHandler : public PermutationCycleHandler<IndexType> {
 public:
  explicit ArrayIndexCycleHandler(DataType* data) : data_(data) {}

  void SetTempFromIndex(IndexType source) override { temp_ = data_[source]; }
  void SetIndexFromIndex(IndexType source,
                         IndexType destination) const override {
    data_[destination] = data_[source];
  }
  void SetIndexFromTemp(IndexType destination) const override {
    data_[destination] = temp_;
  }
  void SetSeen(IndexType* permutation_element) const override {
    *permutation_element = -*permutation_element - 1;
  }
  bool Unseen(IndexType permutation_element) const override {
    return permutation_element >= 0;
  }

 private:
  // Pointer to the base of the array of data to be permuted.
  DataType* data_;

  // Temporary storage for the one extra element we need.
  DataType temp_;

  DISALLOW_COPY_AND_ASSIGN(ArrayIndexCycleHandler);
};

// Note that this template is not implemented in an especially
// performance-sensitive way. In particular, it makes multiple virtual
// method calls for each element of the permutation.
template <typename IndexType>
class PermutationApplier {
 public:
  explicit PermutationApplier(PermutationCycleHandler<IndexType>* cycle_handler)
      : cycle_handler_(cycle_handler) {}

  void Apply(IndexType permutation[], int permutation_start,
             int permutation_end) {
    for (IndexType current = permutation_start; current < permutation_end;
         ++current) {
      IndexType next = permutation[current];
      // cycle_start is only for debugging.
      const IndexType cycle_start = current;
      if (cycle_handler_->Unseen(next)) {
        cycle_handler_->SetSeen(&permutation[current]);
        DCHECK(!cycle_handler_->Unseen(permutation[current]));
        cycle_handler_->SetTempFromIndex(current);
        while (cycle_handler_->Unseen(permutation[next])) {
          cycle_handler_->SetIndexFromIndex(next, current);
          current = next;
          next = permutation[next];
          cycle_handler_->SetSeen(&permutation[current]);
          DCHECK(!cycle_handler_->Unseen(permutation[current]));
        }
        cycle_handler_->SetIndexFromTemp(current);
        // Set current back to the start of this cycle.
        current = next;
      }
      DCHECK_EQ(cycle_start, current);
    }
  }

 private:
  PermutationCycleHandler<IndexType>* cycle_handler_;

  DISALLOW_COPY_AND_ASSIGN(PermutationApplier);
};
}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_PERMUTATION_H_
