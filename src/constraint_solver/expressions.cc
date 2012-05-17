// Copyright 2010-2012 Google
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

#include <math.h>
#include <string.h>
#include <algorithm>
#include "base/hash.h"
#include <string>
#include <utility>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/map-util.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/bitset.h"
#include "util/const_int_array.h"

DEFINE_bool(cp_disable_expression_optimization, false,
            "Disable special optimization when creating expressions.");
DEFINE_bool(cp_share_int_consts, true,
            "Share IntConst's with the same value.");

#if defined(_MSC_VER)
#pragma warning(disable : 4351 4355)
#endif

namespace operations_research {

// ---------- IntExpr ----------

IntVar* IntExpr::VarWithName(const string& name) {
  IntVar* const var = Var();
  var->set_name(name);
  return var;
}

// ---------- IntVar ----------

IntVar::IntVar(Solver* const s) : IntExpr(s) {}

IntVar::IntVar(Solver* const s, const string& name) : IntExpr(s) {
  set_name(name);
}

namespace {
int64* NewUniqueSortedArray(const int64* const values, int* size) {
  int64* new_array = new int64[*size];
  memcpy(new_array, values, (*size) * sizeof(*values));
  std::sort(new_array, new_array + (*size));
  int non_unique = 0;
  for (int i = 0; i < (*size) - 1; ++i) {
    if (new_array[i] == new_array[i + 1]) {
      non_unique++;
    }
  }
  if (non_unique > 0) {
    scoped_array<int64> sorted(new_array);
    DCHECK_GT(*size, 0);
    new_array = new int64[(*size) - non_unique];
    new_array[0] = sorted[0];
    int pos = 1;
    for (int i = 1; i < (*size); ++i) {
      if (sorted[i] != sorted[i - 1]) {
        new_array[pos++] = sorted[i];
      }
    }
    DCHECK_EQ((*size) - non_unique, pos);
    *size = pos;
  }
  return new_array;
}

bool IsArrayActuallySorted(const int64* const values, int size) {
  for (int i = 0; i < size - 1; ++i) {
    if (values[i + 1] < values[i]) {
      return false;
    }
  }
  return true;
}

// ---------- Subclasses of IntVar ----------

// ----- Domain Int Var: base class for variables -----
// It Contains bounds and a bitset representation of possible values.
class DomainIntVar : public IntVar {
 public:
  // Utility classes
    class BitSetIterator : public BaseObject {
     public:
      BitSetIterator(uint64* const bitset, int64 omin)
          : bitset_(bitset),
            omin_(omin),
            max_(kint64min),
            current_(kint64max) {}

      ~BitSetIterator() {}

      void Init(int64 min, int64 max) {
        max_ = max;
        current_ = min;
      }

      bool Ok() const { return current_ <= max_; }

      int64 Value() const { return current_; }

      void Next() {
        if (++current_ <= max_) {
          current_ = UnsafeLeastSignificantBitPosition64(bitset_,
                                                         current_ - omin_,
                                                         max_ - omin_) + omin_;
        }
      }

     private:
      uint64* const bitset_;
      const int64 omin_;
      int64 max_;
      int64 current_;
    };

  class BitSet : public BaseObject {
   public:
    explicit BitSet(Solver* const s) : solver_(s), holes_stamp_(0) {}
    virtual ~BitSet() {}

    virtual int64 ComputeNewMin(int64 nmin, int64 cmin, int64 cmax) = 0;
    virtual int64 ComputeNewMax(int64 nmax, int64 cmin, int64 cmax) = 0;
    virtual bool Contains(int64 val) const = 0;
    virtual bool SetValue(int64 val) = 0;
    virtual bool RemoveValue(int64 val) = 0;
    virtual uint64 Size() const = 0;
    virtual void DelayRemoveValue(int64 val) = 0;
    virtual void ApplyRemovedValues(DomainIntVar* var) = 0;
    virtual void ClearRemovedValues() = 0;
    virtual string pretty_DebugString(int64 min, int64 max) const = 0;
    virtual BitSetIterator* MakeIterator() = 0;

    void InitHoles() {
      const uint64 current_stamp = solver_->stamp();
      if (holes_stamp_ < current_stamp) {
        holes_.clear();
        holes_stamp_ = current_stamp;
      }
    }

    virtual void ClearHoles() {
      holes_.clear();
    }

    const std::vector<int64>& Holes() {
      return holes_;
    }

    void AddHole(int64 value) {
      holes_.push_back(value);
    }

   protected:
    Solver* const solver_;

   private:
    std::vector<int64> holes_;
    uint64 holes_stamp_;
  };

  class QueueHandler : public Demon {
   public:
    explicit QueueHandler(DomainIntVar* const var) : var_(var) {}
    virtual ~QueueHandler() {}
    virtual void Run(Solver* const s) {
      var_->Process();
    }
    virtual Solver::DemonPriority priority() const {
      return Solver::VAR_PRIORITY;
    }
    virtual string DebugString() const {
      return StringPrintf("Handler(%s)", var_->DebugString().c_str());
    }
   private:
    DomainIntVar* const var_;
  };
  // ----- Main Class -----
  DomainIntVar(Solver* const s,
               int64 vmin,
               int64 vmax,
               const string& name);
  DomainIntVar(Solver* const s,
               const std::vector<int64>& sorted_values,
               const string& name);
  virtual ~DomainIntVar();

  virtual int64 Min() const {
    return min_.Value();
  }
  virtual void SetMin(int64 m);
  virtual int64 Max() const {
    return max_.Value();
  }
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const { return (min_.Value() == max_.Value()); }
  virtual int64 Value() const {
    CHECK_EQ(min_.Value(), max_.Value()) << "variable "
                                         << DebugString().c_str()
                                         << "is not bound.";
    return min_.Value();
  }
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  void CreateBits();
  virtual void WhenBound(Demon* d) {
    if (min_.Value() != max_.Value()) {
      bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }
  virtual void WhenRange(Demon* d) {
    if (min_.Value() != max_.Value()) {
      range_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }
  virtual void WhenDomain(Demon* d) {
    if (min_.Value() != max_.Value()) {
      domain_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }

  void Process();
  void Push();
  void ClearInProcess();
  virtual uint64 Size() const {
    if (bits_ != NULL)
      return bits_->Size();
    return (max_.Value() - min_.Value() + 1);
  }
  virtual bool Contains(int64 v) const {
    if (v < min_.Value() || v > max_.Value())
      return false;
    return (bits_ == NULL ? true : bits_->Contains(v));
  }
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const;
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const;
  virtual int64 OldMin() const { return std::min(old_min_, min_.Value()); }
  virtual int64 OldMax() const { return std::max(old_max_, max_.Value()); }

  virtual string DebugString() const;
  BitSet* bitset() const { return bits_; }
  virtual int VarType() const { return DOMAIN_INT_VAR; }
  virtual string BaseName() const { return "IntegerVar"; }

  friend class PlusCstDomainIntVar;
  friend class LinkExprAndDomainIntVar;
 private:
  void CheckOldMin() {
    if (old_min_ > min_.Value()) {
      old_min_ = min_.Value();
    }
  }
  void CheckOldMax() {
    if (old_max_ < max_.Value()) {
      old_max_ = max_.Value();
    }
  }
  Rev<int64> min_;
  Rev<int64> max_;
  int64 old_min_;
  int64 old_max_;
  int64 new_min_;
  int64 new_max_;
  SimpleRevFIFO<Demon*> bound_demons_;
  SimpleRevFIFO<Demon*> range_demons_;
  SimpleRevFIFO<Demon*> domain_demons_;
  QueueHandler handler_;
  bool in_process_;
  BitSet* bits_;
};

// ----- BitSet -----

class SimpleBitSet : public DomainIntVar::BitSet {
 public:
  SimpleBitSet(Solver* const s, int64 vmin, int64 vmax)
      : BitSet(s),
        bits_(NULL),
        stamps_(NULL),
        omin_(vmin),
        omax_(vmax),
        size_(vmax - vmin + 1),
        bsize_(BitLength64(size_.Value())) {
    CHECK_LT(size_.Value(), 0xFFFFFFFF) << "Bitset too large";
    bits_ = new uint64[bsize_];
    stamps_ = new uint64[bsize_];
    for (int i = 0; i < bsize_; ++i) {
      const int bs = (i == size_.Value() - 1) ?
          63 - BitPos64(size_.Value()) :
          0;
      bits_[i] = kAllBits64 >> bs;
      stamps_[i] = s->stamp() - 1;
    }
  }

  SimpleBitSet(Solver* const s,
               const std::vector<int64>& sorted_values,
               int64 vmin,
               int64 vmax)
      : BitSet(s),
        bits_(NULL),
        stamps_(NULL),
        omin_(vmin),
        omax_(vmax),
        size_(sorted_values.size()),
        bsize_(BitLength64(vmax - vmin + 1)) {
    CHECK_LT(size_.Value(), 0xFFFFFFFF) << "Bitset too large";
    bits_ = new uint64[bsize_];
    stamps_ = new uint64[bsize_];
    for (int i = 0; i < bsize_; ++i) {
      bits_[i] = GG_ULONGLONG(0);
      stamps_[i] = s->stamp() - 1;
    }
    for (int i = 0; i < sorted_values.size(); ++i) {
      const int64 val = sorted_values[i];
      DCHECK(!bit(val));
      const int offset = BitOffset64(val - omin_);
      const int pos = BitPos64(val - omin_);
      bits_[offset] |= OneBit64(pos);
    }
  }

  virtual ~SimpleBitSet() {
    delete [] bits_;
    delete [] stamps_;
  }

  bool bit(int64 val) const {
    return IsBitSet64(bits_, val - omin_);
  }

  virtual int64 ComputeNewMin(int64 nmin, int64 cmin, int64 cmax) {
    DCHECK_GE(nmin, omin_);
    DCHECK_LE(nmin, omax_);
    DCHECK_LE(nmin, cmax);
    const int64 new_min =
        UnsafeLeastSignificantBitPosition64(bits_, nmin - omin_, cmax - omin_)
        + omin_;
    const uint64 removed_bits =
        BitCountRange64(bits_, cmin - omin_, new_min - omin_ - 1);
    size_.Add(solver_, -removed_bits);
    return new_min;
  }

  virtual int64 ComputeNewMax(int64 nmax, int64 cmin, int64 cmax) {
    DCHECK_GE(nmax, omin_);
    DCHECK_LE(nmax, omax_);
    const int64 new_max =
        UnsafeMostSignificantBitPosition64(bits_, cmin - omin_, nmax - omin_)
        + omin_;
    const uint64 removed_bits =
        BitCountRange64(bits_, new_max - omin_ + 1, cmax - omin_);
    size_.Add(solver_, -removed_bits);
    return new_max;
  }

  virtual bool SetValue(int64 val) {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    if (bit(val)) {
      size_.SetValue(solver_, 1);
      return true;
    }
    return false;
  }

  virtual bool Contains(int64 val) const {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    return bit(val);
  }

  virtual bool RemoveValue(int64 val) {
    if (val < omin_ || val > omax_ || !bit(val)) {
      return false;
    }
    // Bitset.
    const int64 val_offset = val - omin_;
    const int offset = BitOffset64(val_offset);
    const uint64 current_stamp = solver_->stamp();
    if (stamps_[offset] < current_stamp) {
      stamps_[offset] = current_stamp;
      solver_->SaveValue(&bits_[offset]);
    }
    const int pos = BitPos64(val_offset);
    bits_[offset] &= ~OneBit64(pos);
    // Size.
    size_.Decr(solver_);
    // Holes.
    InitHoles();
    AddHole(val);
    return true;
  }
  virtual uint64 Size() const {
    return size_.Value();
  }

  virtual string DebugString() const {
    string out;
    SStringPrintf(&out, "SimpleBitSet(%" GG_LL_FORMAT
                  "d..%" GG_LL_FORMAT "d : ", omin_, omax_);
    for (int i = 0; i < bsize_; ++i) {
      StringAppendF(&out, "%llx", bits_[i]);
    }
    out += ")";
    return out;
  }

  virtual void DelayRemoveValue(int64 val) {
    removed_.push_back(val);
  }

  virtual void ApplyRemovedValues(DomainIntVar* var) {
    sort(removed_.begin(), removed_.end());
    for (std::vector<int64>::iterator it = removed_.begin();
         it != removed_.end();
         ++it) {
      var->RemoveValue(*it);
    }
  }

  virtual void ClearRemovedValues() {
    removed_.clear();
  }

  virtual string pretty_DebugString(int64 min, int64 max) const {
    string out;
    DCHECK(bit(min));
    DCHECK(bit(max));
    if (max != min) {
      int cumul = true;
      int64 start_cumul = min;
      for (int64 v = min + 1; v < max; ++v) {
        if (bit(v)) {
          if (!cumul) {
            cumul = true;
            start_cumul = v;
          }
        } else {
          if (cumul) {
            if (v == start_cumul + 1) {
              StringAppendF(&out, "%" GG_LL_FORMAT "d ", start_cumul);
            } else if (v == start_cumul + 2) {
              StringAppendF(&out, "%" GG_LL_FORMAT
                            "d %" GG_LL_FORMAT "d ", start_cumul, v - 1);
            } else {
              StringAppendF(&out, "%" GG_LL_FORMAT "d..%"
                            GG_LL_FORMAT "d ", start_cumul, v - 1);
            }
            cumul = false;
          }
        }
      }
      if (cumul) {
        if (max == start_cumul + 1) {
          StringAppendF(&out, "%" GG_LL_FORMAT "d %"
                        GG_LL_FORMAT "d", start_cumul, max);
        } else {
          StringAppendF(&out, "%" GG_LL_FORMAT "d..%"
                        GG_LL_FORMAT "d", start_cumul, max);
        }
      } else {
        StringAppendF(&out, "%" GG_LL_FORMAT "d", max);
      }
    } else {
      StringAppendF(&out, "%" GG_LL_FORMAT "d", min);
    }
    return out;
  }

  virtual DomainIntVar::BitSetIterator* MakeIterator() {
    return new DomainIntVar::BitSetIterator(bits_, omin_);
  }

 private:
  uint64* bits_;
  uint64* stamps_;
  const int64 omin_;
  const int64 omax_;
  NumericalRev<int64> size_;
  const int bsize_;
  std::vector<int64> removed_;
};

// This is a special case where the bitset fits into one 64 bit integer.
// In that case, there are no offset to compute.
class SmallBitSet : public DomainIntVar::BitSet {
 public:
  SmallBitSet(Solver* const s, int64 vmin, int64 vmax)
      : BitSet(s),
        bits_(GG_ULONGLONG(0)),
        stamp_(s->stamp() - 1),
        omin_(vmin), omax_(vmax),
        size_(vmax - vmin + 1) {
    CHECK_LE(size_.Value(), 64) << "Bitset too large";
    bits_ = OneRange64(0, size_.Value() - 1);
  }

  SmallBitSet(Solver* const s,
              const std::vector<int64>& sorted_values,
              int64 vmin,
              int64 vmax)
      : BitSet(s),
        bits_(GG_ULONGLONG(0)),
        stamp_(s->stamp() - 1),
        omin_(vmin),
        omax_(vmax),
        size_(sorted_values.size()) {
    // We know the array is sorted and does not contains duplicate values.
    CHECK_LE(size_.Value(), 64) << "Bitset too large";
    for (int i = 0; i < sorted_values.size(); ++i) {
      const int64 val = sorted_values[i];
      DCHECK(!IsBitSet64(&bits_, val - omin_));
      bits_ |= OneBit64(val - omin_);
    }
  }

  virtual ~SmallBitSet() {}

  bool bit(int64 val) const {
    DCHECK_GE(val - omin_, 0);
    DCHECK_LT(val - omin_, 64);
    return (bits_ & OneBit64(val - omin_)) != 0;
  }

  virtual int64 ComputeNewMin(int64 nmin, int64 cmin, int64 cmax) {
    // We do not clean the bits between cmin and nmin.
    // But we use mask to look only at 'active' bits.

    // Create the mask and compute new bits
    const uint64 new_bits = bits_ & OneRange64(nmin - omin_, cmax - omin_);
    if (new_bits != GG_ULONGLONG(0)) {
      // Compute new size and new min
      size_.SetValue(solver_, BitCount64(new_bits));
      if (bit(nmin)) {  // Common case, the new min is inside the bitset
        return nmin;
      }
      return LeastSignificantBitPosition64(new_bits) + omin_;
    } else {  // == 0 -> Fail()
      solver_->Fail();
      return kint64max;
    }
  }

  virtual int64 ComputeNewMax(int64 nmax, int64 cmin, int64 cmax) {
    // We do not clean the bits between nmax and cmax.
    // But we use mask to look only at 'active' bits.

    // Create the mask and compute new_bits
    const uint64 new_bits = bits_ & OneRange64(cmin - omin_, nmax - omin_);
    if (new_bits != GG_ULONGLONG(0)) {
      // Compute new size and new min
      size_.SetValue(solver_, BitCount64(new_bits));
      if (bit(nmax)) {  // Common case, the new max is inside the bitset
        return nmax;
      }
      return MostSignificantBitPosition64(new_bits) + omin_;
    } else {  // == 0 -> Fail()
      solver_->Fail();
      return kint64min;
    }
  }

  virtual bool SetValue(int64 val) {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    // We do not clean the bits. We will use masks to ignore the bits
    // that should have been cleaned.
    if (bit(val)) {
      size_.SetValue(solver_, 1);
      return true;
    }
    return false;
  }

  virtual bool Contains(int64 val) const {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    return bit(val);
  }

  virtual bool RemoveValue(int64 val) {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    if (bit(val)) {
      // Bitset.
      const uint64 current_stamp = solver_->stamp();
      if (stamp_ < current_stamp) {
        stamp_ = current_stamp;
        solver_->SaveValue(&bits_);
      }
      bits_ &= ~OneBit64(val - omin_);
      DCHECK(!bit(val));
      // Size.
      size_.Decr(solver_);
      // Holes.
      InitHoles();
      AddHole(val);
      return true;
    } else {
      return false;
    }
  }

  virtual uint64 Size() const {
    return size_.Value();
  }

  virtual string DebugString() const {
    return StringPrintf("SmallBitSet(%" GG_LL_FORMAT
                        "d..%" GG_LL_FORMAT "d : %llx)", omin_, omax_, bits_);
  }

  virtual void DelayRemoveValue(int64 val) {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    removed_.push_back(val);
  }

  virtual void ApplyRemovedValues(DomainIntVar* var) {
    sort(removed_.begin(), removed_.end());
    for (std::vector<int64>::iterator it = removed_.begin();
         it != removed_.end();
         ++it) {
      var->RemoveValue(*it);
    }
  }

  virtual void ClearRemovedValues() {
    removed_.clear();
  }


  virtual string pretty_DebugString(int64 min, int64 max) const {
    string out;
    DCHECK(bit(min));
    DCHECK(bit(max));
    if (max != min) {
      int cumul = true;
      int64 start_cumul = min;
      for (int64 v = min + 1; v < max; ++v) {
        if (bit(v)) {
          if (!cumul) {
            cumul = true;
            start_cumul = v;
          }
        } else {
          if (cumul) {
            if (v == start_cumul + 1) {
              StringAppendF(&out, "%" GG_LL_FORMAT "d ", start_cumul);
            } else if (v == start_cumul + 2) {
              StringAppendF(&out, "%" GG_LL_FORMAT
                            "d %" GG_LL_FORMAT "d ", start_cumul, v - 1);
            } else {
              StringAppendF(&out, "%" GG_LL_FORMAT "d..%"
                            GG_LL_FORMAT "d ", start_cumul, v - 1);
            }
            cumul = false;
          }
        }
      }
      if (cumul) {
        if (max == start_cumul + 1) {
          StringAppendF(&out, "%" GG_LL_FORMAT "d %" GG_LL_FORMAT
                        "d", start_cumul, max);
        } else {
          StringAppendF(&out, "%" GG_LL_FORMAT "d..%" GG_LL_FORMAT
                        "d", start_cumul, max);
        }
      } else {
        StringAppendF(&out, "%" GG_LL_FORMAT "d", max);
      }
    } else {
      StringAppendF(&out, "%" GG_LL_FORMAT "d", min);
    }
    return out;
  }

  virtual DomainIntVar::BitSetIterator* MakeIterator() {
    return new DomainIntVar::BitSetIterator(&bits_, omin_);
  }

 private:
  uint64 bits_;
  uint64 stamp_;
  const int64 omin_;
  const int64 omax_;
  NumericalRev<int64> size_;
  std::vector<int64> removed_;
};

class EmptyIterator : public IntVarIterator {
 public:
  virtual ~EmptyIterator() {}
  virtual void Init() {}
  virtual bool Ok() const { return false; }
  virtual int64 Value() const {
    LOG(FATAL) << "Should not be called";
    return 0LL;
  }
  virtual void Next() {}
};

class RangeIterator : public IntVarIterator {
 public:
  explicit RangeIterator(const IntVar* const var)
      : var_(var), min_(kint64max), max_(kint64min), current_(-1) {}

  virtual ~RangeIterator() {}

  virtual void Init() {
    min_ = var_->Min();
    max_ = var_->Max();
    current_ = min_;
  }

  virtual bool Ok() const { return current_ <= max_; }

  virtual int64 Value() const { return current_; }

  virtual void Next() { current_++; }

 private:
  const IntVar* const var_;
  int64 min_;
  int64 max_;
  int64 current_;
};

class DomainIntVarHoleIterator : public IntVarIterator {
 public:
  explicit DomainIntVarHoleIterator(const DomainIntVar* const v)
  : var_(v), bits_(NULL), values_(NULL), size_(0), index_(0) {}

  ~DomainIntVarHoleIterator() {}

  virtual void Init() {
    bits_ = var_->bitset();
    if (bits_ != 0) {
      bits_->InitHoles();
      values_ = bits_->Holes().data();
      size_ = bits_->Holes().size();
    } else {
      values_ = NULL;
      size_ = 0;
    }
    index_ = 0;
  }

  virtual bool Ok() const {
    return index_ < size_;
  }

  virtual int64 Value() const {
    DCHECK(bits_ != NULL);
    DCHECK(index_ < size_);
    return values_[index_];
  }

  virtual void Next() {
    index_++;
  }

 private:
  const DomainIntVar* const var_;
  DomainIntVar::BitSet* bits_;
  const int64* values_;
  int size_;
  int index_;
};

class DomainIntVarDomainIterator : public IntVarIterator {
 public:
  explicit DomainIntVarDomainIterator(const DomainIntVar* const v,
                                      bool reversible)
      :  var_(v),
         bitset_iterator_(NULL),
         min_(kint64max),
         max_(kint64min),
         current_(-1),
         reversible_(reversible) {}

  virtual ~DomainIntVarDomainIterator() {
    if (!reversible_ && bitset_iterator_) {
      delete bitset_iterator_;
    }
  }

  virtual void Init() {
    if (var_->bitset() != NULL && !var_->Bound()) {
      if (reversible_) {
        if (!bitset_iterator_) {
          Solver* const solver = var_->solver();
          solver->SaveValue(reinterpret_cast<void**>(&bitset_iterator_));
          bitset_iterator_ = solver->RevAlloc(var_->bitset()->MakeIterator());
        }
      } else {
        if (bitset_iterator_) {
          delete bitset_iterator_;
        }
        bitset_iterator_ = var_->bitset()->MakeIterator();
      }
      bitset_iterator_->Init(var_->Min(), var_->Max());
    } else {
      if (bitset_iterator_) {
        if (reversible_) {
          Solver* const solver = var_->solver();
          solver->SaveAndSetValue(reinterpret_cast<void**>(&bitset_iterator_),
                                  reinterpret_cast<void*>(NULL));
        } else {
          delete bitset_iterator_;
          bitset_iterator_ = NULL;
        }
      }
      min_ = var_->Min();
      max_ = var_->Max();
      current_ = min_;
    }
  }

  virtual bool Ok() const {
    return bitset_iterator_ ? bitset_iterator_->Ok() : (current_ <= max_);
  }

  virtual int64 Value() const {
    return bitset_iterator_ ? bitset_iterator_->Value() : current_;
  }

  virtual void Next() {
    if (bitset_iterator_) {
      bitset_iterator_->Next();
    } else {
      current_++;
    }
  }

 private:
  const DomainIntVar* const var_;
  DomainIntVar::BitSetIterator* bitset_iterator_;
  int64 min_;
  int64 max_;
  int64 current_;
  const bool reversible_;
};

class UnaryIterator : public IntVarIterator {
 public:
  UnaryIterator(const IntVar* const v, bool hole, bool reversible)
      : iterator_(hole ?
                  v->MakeHoleIterator(reversible) :
                  v->MakeDomainIterator(reversible)),
        reversible_(reversible) {}

  virtual ~UnaryIterator() {
    if (!reversible_) {
      delete iterator_;
    }
  }

  virtual void Init() {
    iterator_->Init();
  }

  virtual bool Ok() const {
    return iterator_->Ok();
  }

  virtual void Next() {
    iterator_->Next();
  }

 protected:
  IntVarIterator* const iterator_;
  const bool reversible_;
};

DomainIntVar::DomainIntVar(Solver* const s,
                           int64 vmin,
                           int64 vmax,
                           const string& name)
    : IntVar(s, name), min_(vmin), max_(vmax), old_min_(vmin),
      old_max_(vmax), new_min_(vmin), new_max_(vmax),
      handler_(this), in_process_(false), bits_(NULL) {}

DomainIntVar::DomainIntVar(Solver* const s,
                           const std::vector<int64>& sorted_values,
                           const string& name)
    : IntVar(s, name), min_(kint64max), max_(kint64min), old_min_(kint64max),
      old_max_(kint64min), new_min_(kint64max), new_max_(kint64min),
      handler_(this), in_process_(false), bits_(NULL) {
  CHECK_GE(sorted_values.size(), 1);
  // We know that the vector is sorted and does not have duplicate values.
  const int64 vmin = sorted_values.front();
  const int64 vmax = sorted_values.back();
  const bool contiguous = vmax - vmin + 1 == sorted_values.size();

  min_.SetValue(solver(), vmin);
  old_min_ = vmin;
  new_min_ = vmin;
  max_.SetValue(solver(), vmax);
  old_max_ = vmax;
  new_max_ = vmax;

  if (!contiguous) {
    if (vmax - vmin + 1 < 65) {
      bits_ = solver()->RevAlloc(new SmallBitSet(solver(),
                                                 sorted_values,
                                                 vmin,
                                                 vmax));
    } else {
      bits_ = solver()->RevAlloc(new SimpleBitSet(solver(),
                                                  sorted_values,
                                                  vmin,
                                                  vmax));
    }
  }
}

DomainIntVar::~DomainIntVar() {}

void DomainIntVar::SetMin(int64 m)  {
  if (m <= min_.Value())
    return;
  if (m > max_.Value())
    solver()->Fail();
  if (in_process_) {
    if (m > new_min_) {
      new_min_ = m;
      if (new_min_ > new_max_) {
        solver()->Fail();
      }
    }
  } else {
    CheckOldMin();
    const int64 new_min = (bits_ == NULL ?
                           m :
                           bits_->ComputeNewMin(m, min_.Value(), max_.Value()));
    min_.SetValue(solver(), new_min);
    if (min_.Value() > max_.Value()) {
      solver()->Fail();
    }
    Push();
  }
}

void DomainIntVar::SetMax(int64 m) {
  if (m >= max_.Value())
    return;
  if (m < min_.Value())
    solver()->Fail();
  if (in_process_) {
    if (m < new_max_) {
      new_max_ = m;
      if (new_max_ < new_min_) {
        solver()->Fail();
      }
    }
  } else {
    CheckOldMax();
    const int64 new_max = (bits_ == NULL ?
                           m :
                           bits_->ComputeNewMax(m, min_.Value(), max_.Value()));
    max_.SetValue(solver(), new_max);
    if (min_.Value() > max_.Value()) {
      solver()->Fail();
    }
    Push();
  }
}

void DomainIntVar::SetRange(int64 mi, int64 ma) {
  if (mi == ma) {
    SetValue(mi);
  } else {
    if (mi > ma || mi > max_.Value() || ma < min_.Value())
      solver()->Fail();
    if (mi <= min_.Value() && ma >= max_.Value())
      return;
    if (in_process_) {
      if (ma < new_max_) {
        new_max_ = ma;
      }
      if (mi > new_min_) {
        new_min_ = mi;
      }
      if (new_min_ > new_max_) {
        solver()->Fail();
      }
    } else {
      if (mi > min_.Value()) {
        CheckOldMin();
        const int64 new_min = (bits_ == NULL ?
                               mi :
                               bits_->ComputeNewMin(mi,
                                                    min_.Value(),
                                                    max_.Value()));
        min_.SetValue(solver(), new_min);
      }
      if (min_.Value() > ma) {
        solver()->Fail();
      }
      if (ma < max_.Value()) {
        CheckOldMax();
        const int64 new_max = (bits_ == NULL ?
                               ma :
                               bits_->ComputeNewMax(ma,
                                                    min_.Value(),
                                                    max_.Value()));
        max_.SetValue(solver(), new_max);
      }
      if (min_.Value() > max_.Value()) {
        solver()->Fail();
      }
      Push();
    }
  }
}

void DomainIntVar::SetValue(int64 v) {
  if (v != min_.Value() || v != max_.Value()) {
    if (v < min_.Value() || v > max_.Value()) {
      solver()->Fail();
    }
    if (in_process_) {
      if (v > new_max_ || v < new_min_) {
        solver()->Fail();
      }
      new_min_ = v;
      new_max_ = v;
    } else {
      if (bits_ && !bits_->SetValue(v)) {
        solver()->Fail();
      }
      CheckOldMin();
      CheckOldMax();
      min_.SetValue(solver(), v);
      max_.SetValue(solver(), v);
      Push();
    }
  }
}

void DomainIntVar::RemoveValue(int64 v)  {
  if (v < min_.Value() || v > max_.Value())
    return;
  if (v == min_.Value()) {
    SetMin(v + 1);
  } else if (v == max_.Value()) {
    SetMax(v - 1);
  } else {
    if (bits_ == NULL) {
      CreateBits();
    }
    if (in_process_) {
      if (v >= new_min_ && v <= new_max_ && bits_->Contains(v)) {
        bits_->DelayRemoveValue(v);
      }
    } else {
      if (bits_->RemoveValue(v)) {
        Push();
      }
    }
  }
}

void DomainIntVar::RemoveInterval(int64 l, int64 u) {
  if (l <= min_.Value()) {
    SetMin(u + 1);
  } else if (u >= max_.Value()) {
    SetMax(l - 1);
  } else {
    for (int64 v = l; v <= u; ++v) {
      RemoveValue(v);
    }
  }
}

void DomainIntVar::CreateBits() {
  solver()->SaveValue(reinterpret_cast<void**>(&bits_));
  if (max_.Value() - min_.Value() < 64) {
    bits_ = solver()->RevAlloc(
        new SmallBitSet(solver(), min_.Value(), max_.Value()));
  } else {
    bits_ = solver()->RevAlloc(
        new SimpleBitSet(solver(), min_.Value(), max_.Value()));
  }
}

void DomainIntVar::ClearInProcess() {
  in_process_ = false;
  if (bits_ != NULL) {
    bits_->ClearHoles();
  }
}

void DomainIntVar::Push() {
  const bool in_process = in_process_;
  Enqueue(&handler_);
  CHECK_EQ(in_process, in_process_);
}

void DomainIntVar::Process() {
  CHECK(!in_process_);
  in_process_ = true;
  if (bits_ != NULL) {
    bits_->ClearRemovedValues();
  }
  SetQueueCleanerOnFail(solver(), this);
  new_min_ = min_.Value();
  new_max_ = max_.Value();
  if (min_.Value() == max_.Value()) {
    for (SimpleRevFIFO<Demon*>::Iterator it(&bound_demons_); it.ok(); ++it) {
      Enqueue(*it);
    }
  }
  if (min_.Value() != OldMin() || max_.Value() != OldMax()) {
    for (SimpleRevFIFO<Demon*>::Iterator it(&range_demons_); it.ok(); ++it) {
      Enqueue(*it);
    }
  }
  for (SimpleRevFIFO<Demon*>::Iterator it(&domain_demons_); it.ok(); ++it) {
    Enqueue(*it);
  }
  ProcessDemonsOnQueue();
  clear_queue_action_on_fail();
  ClearInProcess();
  old_min_ = min_.Value();
  old_max_ = max_.Value();
  if (min_.Value() < new_min_) {
    SetMin(new_min_);
  }
  if (max_.Value() > new_max_) {
    SetMax(new_max_);
  }
  if (bits_ != NULL) {
    bits_->ApplyRemovedValues(this);
  }
}

#define COND_REV_ALLOC(rev, alloc) \
  rev ? solver()->RevAlloc(alloc) : alloc;

IntVarIterator* DomainIntVar::MakeHoleIterator(bool reversible) const {
  return COND_REV_ALLOC(reversible, new DomainIntVarHoleIterator(this));
}

IntVarIterator* DomainIntVar::MakeDomainIterator(bool reversible) const {
  return COND_REV_ALLOC(reversible,
                        new DomainIntVarDomainIterator(this, reversible));
}

string DomainIntVar::DebugString() const {
  string out;
  const string& var_name = name();
  if (!var_name.empty()) {
    out = var_name + "(";
  } else {
    out = "DomainIntVar(";
  }
  if (min_.Value() == max_.Value()) {
    StringAppendF(&out, "%" GG_LL_FORMAT "d", min_.Value());
  } else if (bits_ != NULL) {
    StringAppendF(&out,
                  "%s",
                  bits_->pretty_DebugString(min_.Value(),
                                            max_.Value()).c_str());
  } else {
    StringAppendF(&out,
                  "%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d",
                  min_.Value(),
                  max_.Value());
  }
  out += ")";
  return out;
}

// ----- Boolean variable -----

static const int kUnboundBooleanVarValue = 2;

class BooleanVar : public IntVar {
 public:
  // Utility classes
  class Handler : public Demon {
   public:
    explicit Handler(BooleanVar* const var) : Demon(), var_(var) {}
    virtual ~Handler() {}
    virtual void Run(Solver* const s) {
      var_->Process();
    }
    virtual Solver::DemonPriority priority() const {
      return Solver::VAR_PRIORITY;
    }
    virtual string DebugString() const {
      return StringPrintf("Handler(%s)", var_->DebugString().c_str());
    }
   private:
    BooleanVar* const var_;
  };

  BooleanVar(Solver* const s, const string& name = "")
      : IntVar(s, name), value_(kUnboundBooleanVarValue), handler_(this) {
  }
  virtual ~BooleanVar() {}

  virtual int64 Min() const {
    return (value_ == 1);
  }
  virtual void SetMin(int64 m);
  virtual int64 Max() const {
    return (value_ != 0);
  }
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const { return (value_ != kUnboundBooleanVarValue); }
  virtual int64 Value() const {
    CHECK_NE(value_, kUnboundBooleanVarValue) << "variable is not bound";
    return value_;
  }
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual void WhenBound(Demon* d) {
    if (value_ == kUnboundBooleanVarValue) {
      bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }
  virtual void WhenRange(Demon* d) {
    if (value_ == kUnboundBooleanVarValue) {
      bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }
  virtual void WhenDomain(Demon* d) {
    if (value_ == kUnboundBooleanVarValue) {
      bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }
  void Process();
  void Push() {
    Enqueue(&handler_);
  }
  virtual uint64 Size() const {
    return (1 + (value_ == kUnboundBooleanVarValue));
  }
  virtual bool Contains(int64 v) const {
    return ((v == 0 && value_ != 1) || (v == 1 && value_ != 0));
  }
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new EmptyIterator());
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new RangeIterator(this));
  }
  virtual int64 OldMin() const {
    return 0LL;
  }
  virtual int64 OldMax() const {
    return 1LL;
  }
  virtual string DebugString() const;
  virtual int VarType() const { return BOOLEAN_VAR; }

  void RestoreValue() { value_ = kUnboundBooleanVarValue; }

  virtual string BaseName() const { return "BooleanVar"; }

  friend class TimesBooleanPosIntExpr;
  friend class TimesBooleanIntExpr;
  friend class TimesPosCstBoolVar;

 private:
  int value_;
  SimpleRevFIFO<Demon*> bound_demons_;
  Handler handler_;
};

void BooleanVar::SetMin(int64 m)  {
  if (m <= 0) {
    return;
  }
  if (m > 1)
    solver()->Fail();
  SetValue(1);
}

void BooleanVar::SetMax(int64 m) {
  if (m >= 1) {
    return;
  }
  if (m < 0) {
    solver()->Fail();
  }
  SetValue(0);
}

void BooleanVar::SetRange(int64 mi, int64 ma) {
  if (mi > 1 || ma < 0 || mi > ma) {
    solver()->Fail();
  }
  if (mi == 1) {
    SetValue(1);
  } else if (ma == 0) {
    SetValue(0);
  }
}

void BooleanVar::SetValue(int64 v) {
  if (value_ == kUnboundBooleanVarValue) {
    if (v == 0 || v == 1) {
      InternalSaveBooleanVarValue(solver(), this);
      value_ = static_cast<int>(v);
      Push();
      return;
    }
  } else if (v == value_) {
    return;
  }
  solver()->Fail();
}

void BooleanVar::RemoveValue(int64 v)  {
  if (value_ == kUnboundBooleanVarValue) {
    if (v == 0) {
      SetValue(1);
    } else if (v == 1) {
      SetValue(0);
    }
  } else if (v == value_) {
    solver()->Fail();
  }
}

void BooleanVar::RemoveInterval(int64 l, int64 u) {
  if (l <= 0 && u >= 1) {
    solver()->Fail();
  } else if (l == 1) {
    SetValue(0);
  } else if (u == 0) {
    SetValue(1);
  }
}

void BooleanVar::Process() {
  DCHECK_NE(value_, kUnboundBooleanVarValue);
  for (SimpleRevFIFO<Demon*>::Iterator it(&bound_demons_); it.ok(); ++it) {
    Enqueue(*it);
  }
}

string BooleanVar::DebugString() const {
  string out;
  const string& var_name = name();
  if (!var_name.empty()) {
    out = var_name + "(";
  } else {
    out = "BooleanVar(";
  }
  switch (value_) {
    case 0:
      out += "0";
      break;
    case 1:
      out += "1";
      break;
    case kUnboundBooleanVarValue:
      out += "0 .. 1";
      break;
  }
  out += ")";
  return out;
}

// ----- IntConst -----

class IntConst : public IntVar {
 public:
  IntConst(Solver* const s, int64 value, const string& name = "")
      : IntVar(s, name), value_(value) {}
  virtual ~IntConst() {}

  virtual int64 Min() const {
    return value_;
  }
  virtual void SetMin(int64 m) {
    if (m > value_) {
      solver()->Fail();
    }
  }
  virtual int64 Max() const {
    return value_;
  }
  virtual void SetMax(int64 m) {
    if (m < value_) {
      solver()->Fail();
    }
  }
  virtual void SetRange(int64 l, int64 u) {
    if (l > value_ || u < value_) {
      solver()->Fail();
    }
  }
  virtual void SetValue(int64 v) {
    if (v != value_) {
      solver()->Fail();
    }
  }
  virtual bool Bound() const { return true; }
  virtual int64 Value() const {
    return value_;
  }
  virtual void RemoveValue(int64 v) {
    if (v == value_) {
      solver()->Fail();
    }
  }
  virtual void RemoveInterval(int64 l, int64 u) {
    if (l <= value_ && value_ <= u) {
      solver()->Fail();
    }
  }
  virtual void WhenBound(Demon* d) {}
  virtual void WhenRange(Demon* d) {}
  virtual void WhenDomain(Demon* d) {}
  virtual uint64 Size() const {
    return 1;
  }
  virtual bool Contains(int64 v) const {
    return (v == value_);
  }
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new EmptyIterator());
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new RangeIterator(this));
  }
  virtual int64 OldMin() const {
    return value_;
  }
  virtual int64 OldMax() const {
    return value_;
  }
  virtual string DebugString() const;
  virtual int VarType() const { return CONST_VAR; }

 private:
  int64 value_;
};

string IntConst::DebugString() const {
  string out;
  const string& var_name = name();
  if (!var_name.empty()) {
    SStringPrintf(&out, "%s(%" GG_LL_FORMAT "d)", var_name.c_str(), value_);
  } else {
    SStringPrintf(&out, "IntConst(%" GG_LL_FORMAT "d)", value_);
  }
  return out;
}

// ----- x + c variable, optimized case -----

class PlusCstIntVar : public IntVar {
 public:
  class PlusCstIntVarIterator : public UnaryIterator {
   public:
    PlusCstIntVarIterator(const IntVar* const v, int64 c, bool hole, bool rev)
        : UnaryIterator(v, hole, rev), cst_(c) {}

    virtual ~PlusCstIntVarIterator() {}

    virtual int64 Value() const {
      return iterator_->Value() + cst_;
    }

   private:
    const int64 cst_;
  };
  PlusCstIntVar(Solver* const s, IntVar* v, int64 c);
  virtual ~PlusCstIntVar();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const;
  virtual int64 Value() const;
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual uint64 Size() const;
  virtual bool Contains(int64 v) const;
  virtual void WhenRange(Demon* d);
  virtual void WhenBound(Demon* d);
  virtual void WhenDomain(Demon* d);
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible,
                          new PlusCstIntVarIterator(var_, cst_,
                                                    true, reversible));
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible,
                          new PlusCstIntVarIterator(var_, cst_,
                                                    false, reversible));
  }
  virtual int64 OldMin() const {
    return var_->OldMin() + cst_;
  }
  virtual int64 OldMax() const {
    return var_->OldMax() + cst_;
  }

  virtual string DebugString() const;
  virtual int VarType() const { return VAR_ADD_CST; }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntegerVariable(this,
                                  ModelVisitor::kSumOperation,
                                  cst_,
                                  var_);
  }

 private:
  IntVar* const var_;
  const int64 cst_;
};

PlusCstIntVar::PlusCstIntVar(Solver* const s, IntVar* v, int64 c)
    : IntVar(s), var_(v), cst_(c) {}
PlusCstIntVar::~PlusCstIntVar() {}

int64 PlusCstIntVar::Min() const {
  return var_->Min() + cst_;
}

void PlusCstIntVar::SetMin(int64 m) {
  var_->SetMin(m - cst_);
}

int64 PlusCstIntVar::Max() const {
  return var_->Max() + cst_;
}

void PlusCstIntVar::SetMax(int64 m) {
  var_->SetMax(m - cst_);
}

void PlusCstIntVar::SetRange(int64 l, int64 u) {
  var_->SetRange(l - cst_, u - cst_);
}

void PlusCstIntVar::SetValue(int64 v) {
  var_->SetValue(v - cst_);
  }
bool PlusCstIntVar::Bound() const {
  return var_->Bound();
}

void PlusCstIntVar::WhenRange(Demon* d) {
  var_->WhenRange(d);
}

int64 PlusCstIntVar::Value() const {
  return var_->Value() + cst_;
}

void PlusCstIntVar::RemoveValue(int64 v) {
  var_->RemoveValue(v - cst_);
}

void PlusCstIntVar::RemoveInterval(int64 l, int64 u) {
  var_->RemoveInterval(l - cst_, u - cst_);
}

void PlusCstIntVar::WhenBound(Demon* d) {
  var_->WhenBound(d);
}

void PlusCstIntVar::WhenDomain(Demon* d) {
  var_->WhenDomain(d);
}

uint64 PlusCstIntVar::Size() const {
  return var_->Size();
}

bool PlusCstIntVar::Contains(int64 v) const {
  return var_->Contains(v - cst_);
}

string PlusCstIntVar::DebugString() const {
  return StringPrintf("(%s + %" GG_LL_FORMAT "d)",
                      var_->DebugString().c_str(), cst_);
}

class PlusCstDomainIntVar : public IntVar {
 public:
  class PlusCstDomainIntVarIterator : public UnaryIterator {
   public:
    PlusCstDomainIntVarIterator(const IntVar* const v,
                                int64 c,
                                bool hole,
                                bool reversible)
        : UnaryIterator(v, hole, reversible), cst_(c) {}

    virtual ~PlusCstDomainIntVarIterator() {}

    virtual int64 Value() const {
      return iterator_->Value() + cst_;
    }

   private:
    const int64 cst_;
  };
  PlusCstDomainIntVar(Solver* const s, DomainIntVar* v, int64 c);
  virtual ~PlusCstDomainIntVar();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const;
  virtual int64 Value() const;
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual uint64 Size() const;
  virtual bool Contains(int64 v) const;
  virtual void WhenRange(Demon* d);
  virtual void WhenBound(Demon* d);
  virtual void WhenDomain(Demon* d);
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible,
                          new PlusCstDomainIntVarIterator(var_, cst_,
                                                          true, reversible));
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible,
                          new PlusCstDomainIntVarIterator(var_, cst_,
                                                          false, reversible));
  }
  virtual int64 OldMin() const {
    return var_->OldMin() + cst_;
  }
  virtual int64 OldMax() const {
    return var_->OldMax() + cst_;
  }

  virtual string DebugString() const;
  virtual int VarType() const { return DOMAIN_INT_VAR_ADD_CST; }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntegerVariable(this,
                                  ModelVisitor::kSumOperation,
                                  cst_,
                                  var_);
  }

 private:
  DomainIntVar* const var_;
  const int64 cst_;
};

PlusCstDomainIntVar::PlusCstDomainIntVar(Solver* const s,
                                         DomainIntVar* v,
                                         int64 c)
    : IntVar(s), var_(v), cst_(c) {}
PlusCstDomainIntVar::~PlusCstDomainIntVar() {}

int64 PlusCstDomainIntVar::Min() const {
  return var_->min_.Value() + cst_;
}

void PlusCstDomainIntVar::SetMin(int64 m) {
  var_->DomainIntVar::SetMin(m - cst_);
}

int64 PlusCstDomainIntVar::Max() const {
  return var_->max_.Value() + cst_;
}

void PlusCstDomainIntVar::SetMax(int64 m) {
  var_->DomainIntVar::SetMax(m - cst_);
}

void PlusCstDomainIntVar::SetRange(int64 l, int64 u) {
  var_->DomainIntVar::SetRange(l - cst_, u - cst_);
}

void PlusCstDomainIntVar::SetValue(int64 v) {
  var_->DomainIntVar::SetValue(v - cst_);
}

bool PlusCstDomainIntVar::Bound() const {
  return var_->min_.Value() == var_->max_.Value();
}

void PlusCstDomainIntVar::WhenRange(Demon* d) {
  var_->WhenRange(d);
}

int64 PlusCstDomainIntVar::Value() const {
  CHECK_EQ(var_->min_.Value(), var_->max_.Value()) << "variable is not bound";
  return var_->min_.Value() + cst_;
}

void PlusCstDomainIntVar::RemoveValue(int64 v) {
  var_->DomainIntVar::RemoveValue(v - cst_);
}

void PlusCstDomainIntVar::RemoveInterval(int64 l, int64 u) {
  var_->DomainIntVar::RemoveInterval(l - cst_, u - cst_);
}

void PlusCstDomainIntVar::WhenBound(Demon* d) {
  var_->WhenBound(d);
}

void PlusCstDomainIntVar::WhenDomain(Demon* d) {
  var_->WhenDomain(d);
}

uint64 PlusCstDomainIntVar::Size() const {
  return var_->DomainIntVar::Size();
}

bool PlusCstDomainIntVar::Contains(int64 v) const {
  return var_->DomainIntVar::Contains(v - cst_);
}

string PlusCstDomainIntVar::DebugString() const {
  return StringPrintf("(%s + %" GG_LL_FORMAT "d)",
                      var_->DebugString().c_str(), cst_);
}

// c - x variable, optimized case

class SubCstIntVar : public IntVar {
 public:
  class SubCstIntVarIterator : public UnaryIterator {
   public:
    SubCstIntVarIterator(const IntVar* const v, int64 c, bool hole, bool rev)
        : UnaryIterator(v, hole, rev), cst_(c) {}
    virtual ~SubCstIntVarIterator() {}

    virtual int64 Value() const {
      return cst_ - iterator_->Value();
    }

   private:
    const int64 cst_;
  };

  SubCstIntVar(Solver* const s, IntVar* v, int64 c);
  virtual ~SubCstIntVar();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const;
  virtual int64 Value() const;
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual uint64 Size() const;
  virtual bool Contains(int64 v) const;
  virtual void WhenRange(Demon* d);
  virtual void WhenBound(Demon* d);
  virtual void WhenDomain(Demon* d);
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible,
                          new SubCstIntVarIterator(var_, cst_,
                                                   true, reversible));
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible,
                          new SubCstIntVarIterator(var_, cst_,
                                                   false, reversible));
  }
  virtual int64 OldMin() const {
    return cst_ - var_->OldMax();
  }
  virtual int64 OldMax() const {
    return cst_ - var_->OldMin();
  }
  virtual string DebugString() const;
  virtual int VarType() const { return CST_SUB_VAR; }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntegerVariable(this,
                                  ModelVisitor::kDifferenceOperation,
                                  cst_,
                                  var_);
  }

 private:
  IntVar* const var_;
  const int64 cst_;
};

SubCstIntVar::SubCstIntVar(Solver* const s, IntVar* v, int64 c)
    : IntVar(s), var_(v), cst_(c) {}

SubCstIntVar::~SubCstIntVar() {}

int64 SubCstIntVar::Min() const {
  return cst_ - var_->Max();
}

void SubCstIntVar::SetMin(int64 m) {
  var_->SetMax(cst_ - m);
}

int64 SubCstIntVar::Max() const {
  return cst_ - var_->Min();
}

void SubCstIntVar::SetMax(int64 m) {
  var_->SetMin(cst_ - m);
}

void SubCstIntVar::SetRange(int64 l, int64 u) {
  var_->SetRange(cst_ - u, cst_ - l);
}

void SubCstIntVar::SetValue(int64 v) {
  var_->SetValue(cst_ - v);
  }
bool SubCstIntVar::Bound() const {
  return var_->Bound();
}

void SubCstIntVar::WhenRange(Demon* d) {
  var_->WhenRange(d);
}

int64 SubCstIntVar::Value() const {
  return cst_ - var_->Value();
}

void SubCstIntVar::RemoveValue(int64 v) {
  var_->RemoveValue(cst_ - v);
}

void SubCstIntVar::RemoveInterval(int64 l, int64 u) {
  var_->RemoveInterval(cst_ - u, cst_ - l);
}

void SubCstIntVar::WhenBound(Demon* d) {
  var_->WhenBound(d);
}

void SubCstIntVar::WhenDomain(Demon* d) {
  var_->WhenDomain(d);
}

uint64 SubCstIntVar::Size() const {
  return var_->Size();
}

bool SubCstIntVar::Contains(int64 v) const {
  return var_->Contains(cst_ - v);
}

string SubCstIntVar::DebugString() const {
  return StringPrintf("(%" GG_LL_FORMAT "d - %s)",
                      cst_, var_->DebugString().c_str());
}

// -x variable, optimized case

class OppIntVar : public IntVar {
 public:
  class OppIntVarIterator : public UnaryIterator {
   public:
    OppIntVarIterator(const IntVar* const v, bool hole, bool reversible)
        : UnaryIterator(v, hole, reversible) {}
    virtual ~OppIntVarIterator() {}

    virtual int64 Value() const {
      return -iterator_->Value();
    }
  };

  OppIntVar(Solver* const s, IntVar* v);
  virtual ~OppIntVar();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const;
  virtual int64 Value() const;
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual uint64 Size() const;
  virtual bool Contains(int64 v) const;
  virtual void WhenRange(Demon* d);
  virtual void WhenBound(Demon* d);
  virtual void WhenDomain(Demon* d);
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible,
                          new OppIntVarIterator(var_, true, reversible));
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible,
                          new OppIntVarIterator(var_, false, reversible));
  }
  virtual int64 OldMin() const {
    return -var_->OldMax();
  }
  virtual int64 OldMax() const {
    return -var_->OldMin();
  }
  virtual string DebugString() const;
  virtual int VarType() const { return OPP_VAR; }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntegerVariable(this,
                                  ModelVisitor::kDifferenceOperation,
                                  0,
                                  var_);
  }

 private:
  IntVar* const var_;
};

OppIntVar::OppIntVar(Solver* const s, IntVar* v) : IntVar(s), var_(v) {}

OppIntVar::~OppIntVar() {}

int64 OppIntVar::Min() const {
  return -var_->Max();
}

void OppIntVar::SetMin(int64 m) {
  var_->SetMax(-m);
}

int64 OppIntVar::Max() const {
  return -var_->Min();
}

void OppIntVar::SetMax(int64 m) {
  var_->SetMin(-m);
}

void OppIntVar::SetRange(int64 l, int64 u) {
  var_->SetRange(-u, -l);
}

void OppIntVar::SetValue(int64 v) {
  var_->SetValue(-v);
  }
bool OppIntVar::Bound() const {
  return var_->Bound();
}

void OppIntVar::WhenRange(Demon* d) {
  var_->WhenRange(d);
}

int64 OppIntVar::Value() const {
  return -var_->Value();
}

void OppIntVar::RemoveValue(int64 v) {
  var_->RemoveValue(-v);
}

void OppIntVar::RemoveInterval(int64 l, int64 u) {
  var_->RemoveInterval(-u, -l);
}

void OppIntVar::WhenBound(Demon* d) {
  var_->WhenBound(d);
}

void OppIntVar::WhenDomain(Demon* d) {
  var_->WhenDomain(d);
}

uint64 OppIntVar::Size() const {
  return var_->Size();
}

bool OppIntVar::Contains(int64 v) const {
  return var_->Contains(-v);
}

string OppIntVar::DebugString() const {
  return StringPrintf("-(%s)", var_->DebugString().c_str());
}

// ----- Utility functions -----

int64 PosIntDivUp(int64 e, int64 v) {
  if (e >= 0) {
    return (e + v - 1) / v;
  } else {
    return -(-e / v);
  }
}
int64 PosIntDivDown(int64 e, int64 v) {
  if (e >= 0) {
    return e / v;
  } else {
    return -(-e + v - 1) / v;
  }
}

// x * c variable, optimized case

class TimesPosCstIntVar : public IntVar {
 public:
  class TimesPosCstIntVarIterator : public UnaryIterator {
   public:
    TimesPosCstIntVarIterator(const IntVar* const v,
                              int64 c,
                              bool hole,
                              bool reversible)
        : UnaryIterator(v, hole, reversible), cst_(c) {}
    virtual ~TimesPosCstIntVarIterator() {}

    virtual int64 Value() const {
      return iterator_->Value() * cst_;
    }

   private:
    const int64 cst_;
  };

  TimesPosCstIntVar(Solver* const s, IntVar* v, int64 c);
  virtual ~TimesPosCstIntVar();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const;
  virtual int64 Value() const;
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual uint64 Size() const;
  virtual bool Contains(int64 v) const;
  virtual void WhenRange(Demon* d);
  virtual void WhenBound(Demon* d);
  virtual void WhenDomain(Demon* d);
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible,
                          new TimesPosCstIntVarIterator(var_, cst_,
                                                        true, reversible));
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible,
                          new TimesPosCstIntVarIterator(var_, cst_,
                                                        false, reversible));
  }
  virtual int64 OldMin() const {
    return var_->OldMin() * cst_;
  }
  virtual int64 OldMax() const {
    return var_->OldMax() * cst_;
  }
  virtual string DebugString() const;
  virtual int VarType() const { return VAR_TIMES_POS_CST; }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntegerVariable(this,
                                  ModelVisitor::kProductOperation,
                                  cst_,
                                  var_);
  }

 private:
  IntVar* const var_;
  const int64 cst_;
};

// ----- TimesPosCstIntVar -----

TimesPosCstIntVar::TimesPosCstIntVar(Solver* const s, IntVar* v, int64 c)
    : IntVar(s), var_(v), cst_(c) {}

TimesPosCstIntVar::~TimesPosCstIntVar() {}

int64 TimesPosCstIntVar::Min() const {
  return var_->Min() * cst_;
}

void TimesPosCstIntVar::SetMin(int64 m) {
  var_->SetMin(PosIntDivUp(m, cst_));
}

int64 TimesPosCstIntVar::Max() const {
  return var_->Max() * cst_;
}

void TimesPosCstIntVar::SetMax(int64 m) {
  var_->SetMax(PosIntDivDown(m, cst_));
}

void TimesPosCstIntVar::SetRange(int64 l, int64 u) {
  var_->SetRange(PosIntDivUp(l, cst_), PosIntDivDown(u, cst_));
}

void TimesPosCstIntVar::SetValue(int64 v) {
  if (v % cst_ != 0) {
    solver()->Fail();
  }
  var_->SetValue(v / cst_);
}

bool TimesPosCstIntVar::Bound() const {
  return var_->Bound();
}

void TimesPosCstIntVar::WhenRange(Demon* d) {
  var_->WhenRange(d);
}

int64 TimesPosCstIntVar::Value() const {
  return var_->Value() * cst_;
}

void TimesPosCstIntVar::RemoveValue(int64 v) {
  if (v % cst_ == 0) {
    var_->RemoveValue(v / cst_);
  }
}

void TimesPosCstIntVar::RemoveInterval(int64 l, int64 u) {
  for (int64 v = l; v <= u; ++v) {
    RemoveValue(v);
  }
  // TODO(user) : Improve me
}

void TimesPosCstIntVar::WhenBound(Demon* d) {
  var_->WhenBound(d);
}

void TimesPosCstIntVar::WhenDomain(Demon* d) {
  var_->WhenDomain(d);
}

uint64 TimesPosCstIntVar::Size() const {
  return var_->Size();
}

bool TimesPosCstIntVar::Contains(int64 v) const {
  return (v % cst_ == 0 && var_->Contains(v / cst_));
}

string TimesPosCstIntVar::DebugString() const {
  return StringPrintf("(%s * %" GG_LL_FORMAT "d",
                      var_->DebugString().c_str(), cst_);
}

// b * c variable, optimized case

class TimesPosCstBoolVar : public IntVar {
 public:
  class TimesPosCstBoolVarIterator : public UnaryIterator {
   public:
    // TODO(user) : optimize this.
    TimesPosCstBoolVarIterator(const IntVar* const v,
                               int64 c,
                               bool hole,
                               bool reversible)
        : UnaryIterator(v, hole, reversible), cst_(c) {}
    virtual ~TimesPosCstBoolVarIterator() {}

    virtual int64 Value() const {
      return iterator_->Value() * cst_;
    }

   private:
    const int64 cst_;
  };

  TimesPosCstBoolVar(Solver* const s, BooleanVar* v, int64 c);
  virtual ~TimesPosCstBoolVar();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const;
  virtual int64 Value() const;
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual uint64 Size() const;
  virtual bool Contains(int64 v) const;
  virtual void WhenRange(Demon* d);
  virtual void WhenBound(Demon* d);
  virtual void WhenDomain(Demon* d);
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new EmptyIterator());
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible,
                          new TimesPosCstBoolVarIterator(var_, cst_,
                                                         false, reversible));
  }
  virtual int64 OldMin() const {
    return 0;
  }
  virtual int64 OldMax() const {
    return cst_;
  }
  virtual string DebugString() const;
  virtual int VarType() const { return BOOLEAN_VAR_TIMES_POS_CST; }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntegerVariable(this,
                                  ModelVisitor::kProductOperation,
                                  cst_,
                                  var_);
  }

 private:
  BooleanVar* const var_;
  const int64 cst_;
};

// ----- TimesPosCstBoolVar -----

TimesPosCstBoolVar::TimesPosCstBoolVar(Solver* const s, BooleanVar* v, int64 c)
    : IntVar(s), var_(v), cst_(c) {}

TimesPosCstBoolVar::~TimesPosCstBoolVar() {}

int64 TimesPosCstBoolVar::Min() const {
  return (var_->value_ == 1) * cst_;
}

void TimesPosCstBoolVar::SetMin(int64 m) {
  if (m > cst_) {
    solver()->Fail();
  } else if (m > 0) {
    var_->SetMin(1);
  }
}

int64 TimesPosCstBoolVar::Max() const {
  return (var_->value_ != 0) * cst_;
}

void TimesPosCstBoolVar::SetMax(int64 m) {
  if (m < 0) {
    solver()->Fail();
  } else if (m < cst_) {
    var_->SetMax(0);
  }
}

void TimesPosCstBoolVar::SetRange(int64 l, int64 u) {
  if (u < 0 || l > cst_ || l > u) {
    solver()->Fail();
  }
  if (l > 0) {
    var_->SetMin(1);
  } else if (u < cst_) {
    var_->SetMax(0);
  }
}

void TimesPosCstBoolVar::SetValue(int64 v) {
  if (v == 0) {
    var_->SetValue(0);
  } else if (v == cst_) {
    var_->SetValue(1);
  } else {
    solver()->Fail();
  }
}

bool TimesPosCstBoolVar::Bound() const {
  return var_->value_ != kUnboundBooleanVarValue;
}

void TimesPosCstBoolVar::WhenRange(Demon* d) {
  var_->WhenRange(d);
}

int64 TimesPosCstBoolVar::Value() const {
  CHECK_NE(var_->value_, kUnboundBooleanVarValue) << "variable is not bound";
  return var_->value_ * cst_;
}

void TimesPosCstBoolVar::RemoveValue(int64 v) {
  if (v == 0) {
    var_->RemoveValue(0);
  } else if (v == cst_) {
    var_->RemoveValue(1);
  }
}

void TimesPosCstBoolVar::RemoveInterval(int64 l, int64 u) {
  if (l <= 0 && u >= 0) {
    var_->RemoveValue(0);
  }
  if (l <= cst_ && u >= cst_) {
    var_->RemoveValue(1);
  }
}

void TimesPosCstBoolVar::WhenBound(Demon* d) {
  var_->WhenBound(d);
}

void TimesPosCstBoolVar::WhenDomain(Demon* d) {
  var_->WhenDomain(d);
}

uint64 TimesPosCstBoolVar::Size() const {
  return  (1 + (var_->value_ == kUnboundBooleanVarValue));
}

bool TimesPosCstBoolVar::Contains(int64 v) const {
  if (v == 0) {
    return var_->value_ != 1;
  } else if (v == cst_) {
    return var_->value_ != 0;
  }
  return false;
}

string TimesPosCstBoolVar::DebugString() const {
  return StringPrintf("(%s * %" GG_LL_FORMAT "d)",
                      var_->DebugString().c_str(), cst_);
}

// ---------- arithmetic expressions ----------

// ----- PlusIntExpr -----

class PlusIntExpr : public BaseIntExpr {
 public:
  PlusIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s), left_(l), right_(r) {}
  virtual ~PlusIntExpr() {}
  virtual int64 Min() const {
    return (left_->Min() + right_->Min());
  }
  virtual void SetMin(int64 m) {
    left_->SetMin(m - right_->Max());
    right_->SetMin(m - left_->Max());
  }
  virtual void SetRange(int64 l, int64 u);
  virtual int64 Max() const {
    return (left_->Max() + right_->Max());
  }
  virtual void SetMax(int64 m) {
    left_->SetMax(m - right_->Min());
    right_->SetMax(m - left_->Min());
  }
  virtual bool Bound() const { return (left_->Bound() && right_->Bound()); }
  virtual string name() const {
    return StringPrintf("(%s + %s)",
                        left_->name().c_str(),
                        right_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%s + %s)",
                        left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSum, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument,
                                            left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSum, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

void PlusIntExpr::SetRange(int64 l, int64 u) {
  const int64 left_min = left_->Min();
  const int64 left_max = right_->Min();
  const int64 right_min = left_->Max();
  const int64 right_max = right_->Max();
  if (l > left_min + left_max) {
    left_->SetMin(l - right_max);
    right_->SetMin(l - right_min);
  }
  if (u < right_min + right_max) {
    left_->SetMax(u - left_max);
    right_->SetMax(u - left_min);
  }
}

// ----- PlusIntCstExpr -----

class PlusIntCstExpr : public BaseIntExpr {
 public:
  PlusIntCstExpr(Solver* const s, IntExpr* const e, int64 v)
      : BaseIntExpr(s), expr_(e), value_(v) {}
  virtual ~PlusIntCstExpr() {}
  virtual int64 Min() const {
    return (expr_->Min() + value_);
  }
  virtual void SetMin(int64 m) {
    expr_->SetMin(m - value_);
  }
  virtual int64 Max() const {
    return (expr_->Max() + value_);
  }
  virtual void SetMax(int64 m) {
    expr_->SetMax(m - value_);
  }
  virtual bool Bound() const { return (expr_->Bound()); }
  virtual string name() const {
    return StringPrintf("(%s + %" GG_LL_FORMAT "d)",
                        expr_->name().c_str(), value_);
  }
  virtual string DebugString() const {
    return StringPrintf("(%s + %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), value_);
  }
  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }
  virtual IntVar* CastToVar();
  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSum, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSum, this);
  }

 private:
  IntExpr* const expr_;
  const int64 value_;
};

IntVar* PlusIntCstExpr::CastToVar() {
  Solver* const s = solver();
  IntVar* const var = expr_->Var();
  IntVar* cast = NULL;
  switch (var->VarType()) {
    case DOMAIN_INT_VAR:
      cast = s->RegisterIntVar(s->RevAlloc(
          new PlusCstDomainIntVar(s,
                                  reinterpret_cast<DomainIntVar*>(var),
                                  value_)));
    default:
      cast = s->RegisterIntVar(s->RevAlloc(new PlusCstIntVar(s, var, value_)));
  }
  return cast;
}

// ----- SubIntExpr -----

class SubIntExpr : public BaseIntExpr {
 public:
  SubIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s), left_(l), right_(r) {}
  virtual ~SubIntExpr() {}
  virtual int64 Min() const {
    return (left_->Min() - right_->Max());
  }
  virtual void SetMin(int64 m) {
    left_->SetMin(m + right_->Min());
    right_->SetMax(left_->Max() - m);
  }
  virtual void SetRange(int64 l, int64 u);
  virtual int64 Max() const {
    return (left_->Max() - right_->Min());
  }
  virtual void SetMax(int64 m) {
    left_->SetMax(m + right_->Max());
    right_->SetMin(left_->Min() - m);
  }
  virtual bool Bound() const { return (left_->Bound() && right_->Bound()); }
  virtual string name() const {
    return StringPrintf("(%s - %s)",
                        left_->name().c_str(),
                        right_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%s - %s)",
                        left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kDifference, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kDifference, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

void SubIntExpr::SetRange(int64 l, int64 u) {
  const int64 left_min = left_->Min();
  const int64 left_max = right_->Min();
  const int64 right_min = left_->Max();
  const int64 right_max = right_->Max();
  if (l > left_min - right_max) {
    left_->SetMin(l + left_max);
    right_->SetMax(right_min - l);
  }
  if (u < right_min - left_max) {
    left_->SetMax(u + right_max);
    right_->SetMin(left_min - u);
  }
}

// l - r

// ----- SubIntCstExpr -----

class SubIntCstExpr : public BaseIntExpr {
 public:
  SubIntCstExpr(Solver* const s, IntExpr* const e, int64 v)
      : BaseIntExpr(s), expr_(e), value_(v) {}
  virtual ~SubIntCstExpr() {}
  virtual int64 Min() const {
    return (value_ - expr_->Max());
  }
  virtual void SetMin(int64 m) {
    expr_->SetMax(value_ - m);
  }
  virtual int64 Max() const {
    return (value_ - expr_->Min());
  }
  virtual void SetMax(int64 m) {
    expr_->SetMin(value_ - m);
  }
  virtual bool Bound() const { return (expr_->Bound()); }
  virtual string name() const {
    return StringPrintf("(%" GG_LL_FORMAT "d - %s)",
                        value_, expr_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%" GG_LL_FORMAT "d - %s)",
                        value_, expr_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }
  virtual IntVar* CastToVar();

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kDifference, this);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kDifference, this);
  }

 private:
  IntExpr* const expr_;
  const int64 value_;
};

IntVar* SubIntCstExpr::CastToVar() {
  Solver* const s = solver();
  IntVar* const var =
      s->RegisterIntVar(s->RevAlloc(new SubCstIntVar(s, expr_->Var(), value_)));
  return var;
}

// ----- OppIntExpr -----

class OppIntExpr : public BaseIntExpr {
 public:
  OppIntExpr(Solver* const s, IntExpr* const e) : BaseIntExpr(s), expr_(e) {}
  virtual ~OppIntExpr() {}
  virtual int64 Min() const {
    return (-expr_->Max());
  }
  virtual void SetMin(int64 m) {
    expr_->SetMax(-m);
  }
  virtual int64 Max() const {
    return (-expr_->Min());
  }
  virtual void SetMax(int64 m) {
    expr_->SetMin(-m);
  }
  virtual bool Bound() const { return (expr_->Bound()); }
  virtual string name() const {
    return StringPrintf("(-%s)", expr_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(-%s)", expr_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }
  virtual IntVar* CastToVar();

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kOpposite, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kOpposite, this);
  }

 private:
  IntExpr* const expr_;
};

IntVar* OppIntExpr::CastToVar() {
  Solver* const s = solver();
  IntVar* const var =
      s->RegisterIntVar(s->RevAlloc(new OppIntVar(s, expr_->Var())));
  return var;
}

// ----- TimesIntPosCstExpr -----

class TimesIntPosCstExpr : public BaseIntExpr {
 public:
  TimesIntPosCstExpr(Solver* const s, IntExpr* const e, int64 v);
  virtual ~TimesIntPosCstExpr();
  virtual int64 Min() const {
    return (expr_->Min() * value_);
  }
  virtual void SetMin(int64 m);
  virtual int64 Max() const {
    return (expr_->Max() * value_);
  }
  virtual void SetMax(int64 m);
  virtual bool Bound() const { return (expr_->Bound()); }
  virtual string name() const {
    return StringPrintf("(%s * %" GG_LL_FORMAT "d)",
                        expr_->name().c_str(), value_);
  }
  virtual string DebugString() const {
    return StringPrintf("(%s * %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), value_);
  }
  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }
  virtual IntVar* CastToVar();

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
  }

 private:
  IntExpr* const expr_;
  const int64 value_;
};

TimesIntPosCstExpr::TimesIntPosCstExpr(Solver* const s,
                                       IntExpr* const e,
                                       int64 v)
    : BaseIntExpr(s), expr_(e), value_(v) {
  CHECK_GE(v, 0);
}

TimesIntPosCstExpr::~TimesIntPosCstExpr() {}

void TimesIntPosCstExpr::SetMin(int64 m) {
  expr_->SetMin(PosIntDivUp(m, value_));
}

void TimesIntPosCstExpr::SetMax(int64 m) {
  expr_->SetMax(PosIntDivDown(m, value_));
}

IntVar* TimesIntPosCstExpr::CastToVar() {
  Solver* const s = solver();
  IntVar* var = NULL;
  if (expr_->IsVar() &&
      reinterpret_cast<IntVar*>(expr_)->VarType() == BOOLEAN_VAR) {
    var = s->RegisterIntVar(s->RevAlloc(
        new TimesPosCstBoolVar(s,
                               reinterpret_cast<BooleanVar*>(expr_),
                               value_)));
  } else {
    var = s->RegisterIntVar(
        s->RevAlloc(new TimesPosCstIntVar(s, expr_->Var(), value_)));
  }
  return var;
}

// ----- TimesIntNegCstExpr -----

class TimesIntNegCstExpr : public BaseIntExpr {
 public:
  TimesIntNegCstExpr(Solver* const s, IntExpr* const e, int64 v);
  virtual ~TimesIntNegCstExpr();
  virtual int64 Min() const {
    return (expr_->Max() * value_);
  }
  virtual void SetMin(int64 m);
  virtual int64 Max() const {
    return (expr_->Min() * value_);
  }
  virtual void SetMax(int64 m);
  virtual bool Bound() const { return (expr_->Bound()); }
  virtual string name() const {
    return StringPrintf("(%s * %" GG_LL_FORMAT "d)",
                        expr_->name().c_str(), value_);
  }
  virtual string DebugString() const {
    return StringPrintf("(%s * %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), value_);
  }
  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
  }

 private:
  IntExpr* const expr_;
  const int64 value_;
};

TimesIntNegCstExpr::TimesIntNegCstExpr(Solver* const s,
                                       IntExpr* const e,
                                       int64 v)
    : BaseIntExpr(s), expr_(e), value_(v) {
  CHECK_LE(v, 0);
}

TimesIntNegCstExpr::~TimesIntNegCstExpr() {}

void TimesIntNegCstExpr::SetMin(int64 m) {
  expr_->SetMax(PosIntDivDown(-m, -value_));
}

void TimesIntNegCstExpr::SetMax(int64 m) {
  expr_->SetMin(PosIntDivUp(-m, -value_));
}


// ----- Utilities for product expression -----

// Propagates set_min on left * right, left and right >= 0.
void SetPosPosMinExpr(IntExpr* const left, IntExpr* const right, int64 m) {
  DCHECK_GE(left->Min(), 0);
  DCHECK_GE(right->Min(), 0);
  const int64 lmax = left->Max();
  const int64 rmax = right->Max();
  if (m > lmax * rmax) {
    left->solver()->Fail();
  }
  if (m > left->Min() * right->Min()) {
    // Ok for m == 0 due to left and right being positive
    if (0 != rmax) {
      left->SetMin(PosIntDivUp(m, rmax));
    }
    if (0 != lmax) {
      right->SetMin(PosIntDivUp(m, lmax));
    }
  }
}

// Propagates set_max on left * right, left and right >= 0.
void SetPosPosMaxExpr(IntExpr* const left, IntExpr* const right, int64 m) {
  DCHECK_GE(left->Min(), 0);
  DCHECK_GE(right->Min(), 0);
  const int64 lmin = left->Min();
  const int64 rmin = right->Min();
  if (m < lmin * rmin) {
    left->solver()->Fail();
  }
  if (m < left->Max() * right->Max()) {
    if (0 != lmin) {
      right->SetMax(PosIntDivDown(m, lmin));
    }
    if (0 != rmin) {
      left->SetMax(PosIntDivDown(m, rmin));
    }
    // else do nothing: 0 is supporting any value from other expr.
  }
}

// Propagates set_min on left * right, left >= 0, right across 0.
void SetPosGenMinExpr(IntExpr* const left, IntExpr* const right, int64 m) {
  DCHECK_GE(left->Min(), 0);
  DCHECK_GT(right->Max(), 0);
  DCHECK_LT(right->Min(), 0);
  const int64 lmax = left->Max();
  const int64 rmax = right->Max();
  if (m > lmax * rmax) {
    left->solver()->Fail();
  }
  DCHECK_GT(left->Max(), 0);
  if (m > 0) {  // We deduce right > 0.
    left->SetMin(PosIntDivUp(m, rmax));
    right->SetMin(PosIntDivUp(m, lmax));
  } else if (m == 0) {
    const int64 lmin = left->Min();
    if (lmin > 0) {
      right->SetMin(0);
    }
  } else {  // m < 0
    const int64 lmin = left->Min();
    if (0 != lmin) {  // We cannot deduce anything if 0 is in the domain.
      right->SetMin(-PosIntDivDown(-m, lmin));
    }
  }
}

// Propagates set_min on left * right, left and right across 0.
void SetGenGenMinExpr(IntExpr* const left, IntExpr* const right, int64 m) {
  DCHECK_LT(left->Min(), 0);
  DCHECK_GT(left->Max(), 0);
  DCHECK_GT(right->Max(), 0);
  DCHECK_LT(right->Min(), 0);
  const int64 lmin = left->Min();
  const int64 lmax = left->Max();
  const int64 rmin = right->Min();
  const int64 rmax = right->Max();
  if (m > std::max(lmin * rmin, lmax * rmax)) {
    left->solver()->Fail();
  }
  if (m > lmin * rmin) {  // Must be positive section * positive section.
    left->SetMin(PosIntDivUp(m, rmax));
    right->SetMin(PosIntDivUp(m, lmax));
  } else if (m > lmax * rmax) {  // Negative section * negative section.
    left->SetMax(-PosIntDivUp(m, -rmin));
    right->SetMax(-PosIntDivUp(m, -lmin));
  }
}

void TimesSetMin(IntExpr* const left,
                 IntExpr* const right,
                 IntExpr* const minus_left,
                 IntExpr* const minus_right,
                 int m) {
  if (left->Min() >= 0) {
    if (right->Min() >= 0) {
      SetPosPosMinExpr(left, right, m);
    } else if (right->Max() <= 0) {
      SetPosPosMaxExpr(left, minus_right, -m);
    } else {  // right->Min() < 0 && right->Max() > 0
      SetPosGenMinExpr(left, right, m);
    }
  } else if (left->Max() <= 0) {
    if (right->Min() >= 0) {
      SetPosPosMaxExpr(right, minus_left, -m);
    } else if (right->Max() <= 0) {
      SetPosPosMinExpr(minus_left, minus_right, m);
    } else {  // right->Min() < 0 && right->Max() > 0
      SetPosGenMinExpr(minus_left, minus_right, m);
    }
  } else if (right->Min() >= 0) {  // left->Min() < 0 && left->Max() > 0
    SetPosGenMinExpr(right, left, m);
  } else if (right->Max() <= 0) {  // left->Min() < 0 && left->Max() > 0
    SetPosGenMinExpr(minus_right, minus_left, m);
  } else {  // left->Min() < 0 && left->Max() > 0 &&
            // right->Min() < 0 && right->Max() > 0
    SetGenGenMinExpr(left, right, m);
  }
}

class TimesIntExpr : public BaseIntExpr {
 public:
  TimesIntExpr(Solver* const s,
               IntExpr* const l,
               IntExpr* const r)
      : BaseIntExpr(s),
        left_(l),
        right_(r),
        minus_left_(s->MakeOpposite(left_)),
        minus_right_(s->MakeOpposite(right_)) {}
  virtual ~TimesIntExpr() {}
  virtual int64 Min() const {
    const int64 lmin = left_->Min();
    const int64 lmax = left_->Max();
    const int64 rmin = right_->Min();
    const int64 rmax = right_->Max();
    return std::min(std::min(lmin * rmin, lmax * rmax),
                    std::min(lmax * rmin, lmin * rmax));
  }
  virtual void SetMin(int64 m);
  virtual int64 Max() const {
    const int64 lmin = left_->Min();
    const int64 lmax = left_->Max();
    const int64 rmin = right_->Min();
    const int64 rmax = right_->Max();
    return std::max(std::max(lmin * rmin, lmax * rmax),
                    std::max(lmax * rmin, lmin * rmax));
  }
  virtual void SetMax(int64 m);
  virtual bool Bound() const;
  virtual string name() const {
    return StringPrintf("(%s * %s)",
                        left_->name().c_str(),
                        right_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%s * %s)",
                        left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  IntExpr* const minus_left_;
  IntExpr* const minus_right_;
};

void TimesIntExpr::SetMin(int64 m) {
  TimesSetMin(left_, right_, minus_left_, minus_right_, m);
}

void TimesIntExpr::SetMax(int64 m) {
  TimesSetMin(left_, minus_right_, minus_left_, right_, -m);
}

bool TimesIntExpr::Bound() const {
  const bool left_bound = left_->Bound();
  const bool right_bound = right_->Bound();
  return ((left_bound && left_->Max() == 0) ||
          (right_bound && right_->Max() == 0) ||
          (left_bound && right_bound));
}

// ----- TimesIntPosExpr -----

class TimesIntPosExpr : public BaseIntExpr {
 public:
  TimesIntPosExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s), left_(l), right_(r) {}
  virtual ~TimesIntPosExpr() {}
  virtual int64 Min() const {
    return (left_->Min() * right_->Min());
  }
  virtual void SetMin(int64 m);
  virtual int64 Max() const {
    return (left_->Max() * right_->Max());
  }
  virtual void SetMax(int64 m);
  virtual bool Bound() const;
  virtual string name() const {
    return StringPrintf("(%s * %s)",
                        left_->name().c_str(),
                        right_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%s * %s)",
                        left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

void TimesIntPosExpr::SetMin(int64 m) {
  SetPosPosMinExpr(left_, right_, m);
}

void TimesIntPosExpr::SetMax(int64 m) {
  SetPosPosMaxExpr(left_, right_, m);
}

bool TimesIntPosExpr::Bound() const {
  return (left_->Max() == 0 ||
          right_->Max() == 0 ||
          (left_->Bound() && right_->Bound()));
}

// ----- TimesBooleanPosIntExpr -----

class TimesBooleanPosIntExpr : public BaseIntExpr {
 public:
  TimesBooleanPosIntExpr(Solver* const s, BooleanVar* const b, IntExpr* const e)
      : BaseIntExpr(s), boolvar_(b), expr_(e) {}
  virtual ~TimesBooleanPosIntExpr() {}
  virtual int64 Min() const {
    return (boolvar_->value_ == 1 ? expr_->Min() : 0);
  }
  virtual void SetMin(int64 m);
  virtual int64 Max() const {
    return (boolvar_->value_ == 0 ? 0 : expr_->Max());
  }
  virtual void SetMax(int64 m);
  virtual void Range(int64* mi, int64* ma);
  virtual void SetRange(int64 mi, int64 ma);
  virtual bool Bound() const;
  virtual string name() const {
    return StringPrintf("(%s * %s)",
                        boolvar_->name().c_str(),
                        expr_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%s * %s)",
                        boolvar_->DebugString().c_str(),
                        expr_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    boolvar_->WhenRange(d);
    expr_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument,
                                            boolvar_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
  }

 private:
  BooleanVar* const boolvar_;
  IntExpr* const expr_;
};

void TimesBooleanPosIntExpr::SetMin(int64 m) {
  if (m > 0) {
    boolvar_->SetValue(1);
    expr_->SetMin(m);
  }
}

void TimesBooleanPosIntExpr::SetMax(int64 m) {
  if (m < 0) {
    solver()->Fail();
  }
  if (m < expr_->Min()) {
    boolvar_->SetValue(0);
  }
  if (boolvar_->value_ == 1) {
    expr_->SetMax(m);
  }
}

void TimesBooleanPosIntExpr::Range(int64* mi, int64* ma) {
  const int value = boolvar_->value_;
  if (value == 0) {
    *mi = 0;
    *ma = 0;
  } else if (value == 1) {
    expr_->Range(mi, ma);
  } else {
    *mi = 0;
    *ma = expr_->Max();
  }
}

void TimesBooleanPosIntExpr::SetRange(int64 mi, int64 ma) {
  if (ma < 0 || mi > ma) {
    solver()->Fail();
  }
  if (mi > 0) {
    boolvar_->SetValue(1);
    expr_->SetMin(mi);
  }
  if (ma < expr_->Min()) {
    boolvar_->SetValue(0);
  }
  if (boolvar_->value_ == 1) {
    expr_->SetMax(ma);
  }
}

bool TimesBooleanPosIntExpr::Bound() const {
  return (boolvar_->value_ == 0 ||
          expr_->Max() == 0 ||
          (boolvar_->value_ != kUnboundBooleanVarValue && expr_->Bound()));
}

// ----- TimesBooleanIntExpr -----

class TimesBooleanIntExpr : public BaseIntExpr {
 public:
  TimesBooleanIntExpr(Solver* const s, BooleanVar* const b, IntExpr* const e)
      : BaseIntExpr(s), boolvar_(b), expr_(e) {}
  virtual ~TimesBooleanIntExpr() {}
  virtual int64 Min() const {
    switch (boolvar_->value_) {
      case 0: {
        return 0LL;
      }
      case 1: {
        return expr_->Min();
      }
      default: {
        DCHECK_EQ(kUnboundBooleanVarValue, boolvar_->value_);
        return std::min(0LL, expr_->Min());
      }
    }
  }
  virtual void SetMin(int64 m);
  virtual int64 Max() const {
    switch (boolvar_->value_) {
      case 0: {
        return 0LL;
      }
      case 1: {
        return expr_->Max();
      }
      default: {
        DCHECK_EQ(kUnboundBooleanVarValue, boolvar_->value_);
        return std::max(0LL, expr_->Max());
      }
    }
  }
  virtual void SetMax(int64 m);
  virtual void Range(int64* mi, int64* ma);
  virtual void SetRange(int64 mi, int64 ma);
  virtual bool Bound() const;
  virtual string name() const {
    return StringPrintf("(%s * %s)",
                        boolvar_->name().c_str(),
                        expr_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%s * %s)",
                        boolvar_->DebugString().c_str(),
                        expr_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    boolvar_->WhenRange(d);
    expr_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument,
                                            boolvar_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
  }

 private:
  BooleanVar* const boolvar_;
  IntExpr* const expr_;
};

void TimesBooleanIntExpr::SetMin(int64 m) {
  switch (boolvar_->value_) {
    case 0: {
      if (m > 0) {
        solver()->Fail();
      }
      break;
    }
    case 1: {
      expr_->SetMin(m);
      break;
    }
    default: {
      DCHECK_EQ(kUnboundBooleanVarValue, boolvar_->value_);
      if (m > 0) {  // 0 is no longer possible for boolvar because min > 0.
        boolvar_->SetValue(1);
        expr_->SetMin(m);
      } else if (m <= 0 && expr_->Max() < m) {
        boolvar_->SetValue(0);
      }
    }
  }
}

void TimesBooleanIntExpr::SetMax(int64 m) {
  switch (boolvar_->value_) {
    case 0: {
      if (m < 0) {
        solver()->Fail();
      }
      break;
    }
    case 1: {
      expr_->SetMax(m);
      break;
    }
    default: {
      DCHECK_EQ(kUnboundBooleanVarValue, boolvar_->value_);
      if (m < 0) {  // 0 is no longer possible for boolvar because max < 0.
        boolvar_->SetValue(1);
        expr_->SetMax(m);
      } else if (m >= 0 && expr_->Min() > m) {
        boolvar_->SetValue(0);
      }
    }
  }
}

void TimesBooleanIntExpr::Range(int64* mi, int64* ma) {
  switch (boolvar_->value_) {
    case 0: {
      *mi = 0;
      *ma = 0;
      break;
    }
    case 1: {
      *mi = expr_->Min();
      *ma = expr_->Max();
      break;
    }
    default: {
      DCHECK_EQ(kUnboundBooleanVarValue, boolvar_->value_);
      *mi = std::min(0LL, expr_->Min());
      *ma = std::max(0LL, expr_->Max());
      break;
    }
  }
}

void TimesBooleanIntExpr::SetRange(int64 mi, int64 ma) {
  if (mi > ma) {
    solver()->Fail();
  }
  switch (boolvar_->value_) {
    case 0: {
      if (mi > 0 || ma < 0) {
        solver()->Fail();
      }
      break;
    }
    case 1: {
      expr_->SetRange(mi, ma);
      break;
    }
    default: {
      DCHECK_EQ(kUnboundBooleanVarValue, boolvar_->value_);
      if (mi > 0) {
        boolvar_->SetValue(1);
        expr_->SetMin(mi);
      } else if (mi == 0 && expr_->Max() < 0) {
        boolvar_->SetValue(0);
      }
      if (ma < 0) {
        boolvar_->SetValue(1);
        expr_->SetMax(ma);
      } else if (ma == 0 && expr_->Min() > 0) {
        boolvar_->SetValue(0);
      }
      break;
    }
  }
}

bool TimesBooleanIntExpr::Bound() const {
  return (boolvar_->value_ == 0 ||
          (expr_->Bound() && (
              boolvar_->value_ != kUnboundBooleanVarValue ||
              expr_->Max() == 0)));
}

// ----- DivIntPosCstExpr -----

class DivIntPosCstExpr : public BaseIntExpr {
 public:
  DivIntPosCstExpr(Solver* const s, IntExpr* const e, int64 v)
      : BaseIntExpr(s), expr_(e), value_(v) {
    CHECK_GE(v, 0);
  }
  virtual ~DivIntPosCstExpr() {}
  virtual int64 Min() const {
    return PosIntDivDown(expr_->Min(), value_);
  }
  virtual void SetMin(int64 m) {
    expr_->SetMin(m * value_);
  }
  virtual int64 Max() const {
    return PosIntDivDown(expr_->Max(), value_);
  }
  virtual void SetMax(int64 m) {
    expr_->SetMax((m + 1) * value_ - 1);
  }
  virtual string name() const {
    return StringPrintf("(%s div %" GG_LL_FORMAT "d)",
                        expr_->name().c_str(), value_);
  }
  virtual string DebugString() const {
    return StringPrintf("(%s div %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), value_);
  }
  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kDivide, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kDivide, this);
  }

 private:
  IntExpr* const expr_;
  const int64 value_;
};

// ----- IntAbs ------

class IntAbs : public BaseIntExpr {
 public:
  IntAbs(Solver* const s, IntExpr* const e) : BaseIntExpr(s), expr_(e) {}
  virtual ~IntAbs() {}

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m) {
    expr_->SetRange(-m, m);
  }
  virtual bool Bound() const {
    return expr_->Bound();
  }
  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }
  virtual string name() const {
    return StringPrintf("IntAbs(%s)", expr_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("IntAbs(%s)", expr_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kAbs, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kAbs, this);
  }

 private:
  IntExpr* const expr_;
};

void IntAbs::SetMin(int64 m) {
  const int64 emin = expr_->Min();
  const int64 emax = expr_->Max();
  if (emin >= 0) {
    expr_->SetMin(m);
  } else if (emax <= 0) {
    expr_->SetMax(-m);
  } else if (expr_->IsVar()) {
    reinterpret_cast<IntVar*>(expr_)->RemoveInterval(-m + 1, m - 1);
  }
}

int64 IntAbs::Min() const {
  const int64 emin = expr_->Min();
  const int64 emax = expr_->Max();
  if (emin >= 0) {
    return emin;
  }
  if (emax <= 0) {
    return -emax;
  }
  return 0;
}

int64 IntAbs::Max() const {
  const int64 emin = expr_->Min();
  const int64 emax = expr_->Max();
  if (emin >= 0) {
    return emax;
  }
  if (emax <= 0) {
    return -emin;
  }
  return std::max(-emin, emax);
}

// ----- Square -----

class IntSquare : public BaseIntExpr {
 public:
  IntSquare(Solver* const s, IntExpr* const e) : BaseIntExpr(s), expr_(e) {}
  virtual ~IntSquare() {}

  virtual int64 Min() const {
    const int64 emin = expr_->Min();
    if (emin >= 0) {
      return emin * emin;
    }
    const int64 emax = expr_->Max();
    if (emax < 0) {
      return emax * emax;
    }
    return 0LL;
  }
  virtual void SetMin(int64 m) {
    if (m <= 0) {
      return;
    }
    const int64 emin = expr_->Min();
    const int64 emax = expr_->Max();
    const int64 root = static_cast<int64>(ceil(sqrt(static_cast<double>(m))));
    if (emin >= 0) {
      expr_->SetMin(root);
    } else if (emax <= 0) {
      expr_->SetMax(-root);
    } else if (expr_->IsVar()) {
      reinterpret_cast<IntVar*>(expr_)->RemoveInterval(-root + 1, root - 1);
    }
  }
  virtual int64 Max() const {
    const int64 emax = expr_->Max();
    const int64 emin = expr_->Min();
    return std::max(emin * emin, emax * emax);
  }
  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    const int64 root = static_cast<int64>(floor(sqrt(static_cast<double>(m))));
    expr_->SetRange(-root, root);
  }
  virtual bool Bound() const {
    return expr_->Bound();
  }
  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }
  virtual string name() const {
    return StringPrintf("IntSquare(%s)", expr_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("IntSquare(%s)", expr_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSquare, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSquare, this);
  }

 private:
  IntExpr* const expr_;
};

class PosIntSquare : public BaseIntExpr {
 public:
  PosIntSquare(Solver* const s, IntExpr* const e) : BaseIntExpr(s), expr_(e) {}
  virtual ~PosIntSquare() {}

  virtual int64 Min() const {
    const int64 emin = expr_->Min();
    return emin * emin;
  }
  virtual void SetMin(int64 m) {
    if (m <= 0) {
      return;
    }
    const int64 root = static_cast<int64>(ceil(sqrt(static_cast<double>(m))));
    expr_->SetMin(root);
  }
  virtual int64 Max() const {
    const int64 emax = expr_->Max();
    return emax * emax;
  }
  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    const int64 root = static_cast<int64>(floor(sqrt(static_cast<double>(m))));
    expr_->SetMax(root);
  }
  virtual bool Bound() const {
    return expr_->Bound();
  }
  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }
  virtual string name() const {
    return StringPrintf("PosIntSquare(%s)", expr_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("PosIntSquare(%s)", expr_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSquare, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSquare, this);
  }

 private:
  IntExpr* const expr_;
};

// ----- Min(expr, expr) -----

class MinIntExpr : public BaseIntExpr {
 public:
  MinIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s), left_(l), right_(r) {}
  virtual ~MinIntExpr() {}
  virtual int64 Min() const {
    const int64 lmin = left_->Min();
    const int64 rmin = right_->Min();
    return std::min(lmin, rmin);
  }
  virtual void SetMin(int64 m) {
    left_->SetMin(m);
    right_->SetMin(m);
  }
  virtual int64 Max() const {
    const int64 lmax = left_->Max();
    const int64 rmax = right_->Max();
    return std::min(lmax, rmax);
  }
  virtual void SetMax(int64 m) {
    if (left_->Min() > m) {
      right_->SetMax(m);
    }
    if (right_->Min() > m) {
      left_->SetMax(m);
    }
  }
  virtual string name() const {
    return StringPrintf("MinIntExpr(%s, %s)", left_->name().c_str(),
                        right_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("MinIntExpr(%s, %s)",
                        left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kMin, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument,
                                            left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kMin, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

// ----- Min(expr, constant) -----

class MinCstIntExpr : public BaseIntExpr {
 public:
  MinCstIntExpr(Solver* const s, IntExpr* const e, int64 v)
      : BaseIntExpr(s), expr_(e), value_(v) {}
  virtual ~MinCstIntExpr() {}
  virtual int64 Min() const {
    const int64 emin = expr_->Min();
    return std::min(emin, value_);
  }
  virtual void SetMin(int64 m) {
    if (m > value_) {
      solver()->Fail();
    }
    expr_->SetMin(m);
  }
  virtual int64 Max() const {
    const int64 emax = expr_->Max();
    return std::min(emax, value_);
  }
  virtual void SetMax(int64 m) {
    if (value_ > m) {
      expr_->SetMax(m);
    }
  }
  virtual bool Bound() const {
    return (expr_->Bound() || expr_->Min() >= value_);
  }
  virtual string name() const {
    return StringPrintf("MinCstIntExpr(%s, %" GG_LL_FORMAT "d)",
                        expr_->name().c_str(), value_);
  }
  virtual string DebugString() const {
    return StringPrintf("MinCstIntExpr(%s, %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), value_);
  }
  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kMin, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kMin, this);
  }

 private:
  IntExpr* const expr_;
  const int64 value_;
};

// ----- Max(expr, expr) -----

class MaxIntExpr : public BaseIntExpr {
 public:
  MaxIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s), left_(l), right_(r) {}
  virtual ~MaxIntExpr() {}
  virtual int64 Min() const {
    const int64 lmin = left_->Min();
    const int64 rmin = right_->Min();
    return std::max(lmin, rmin);
  }
  virtual void SetMin(int64 m) {
    const int64 lmax = left_->Max();
    if (lmax < m) {
      right_->SetMin(m);
    }
    const int64 rmax = right_->Max();
    if (rmax < m) {
      left_->SetMin(m);
    }
  }
  virtual int64 Max() const {
    const int64 lmax = left_->Max();
    const int64 rmax = right_->Max();
    return std::max(lmax, rmax);
  }
  virtual void SetMax(int64 m) {
    left_->SetMax(m);
    right_->SetMax(m);
  }
  virtual string name() const {
    return StringPrintf("MaxIntExpr(%s, %s)", left_->name().c_str(),
                        right_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("MaxIntExpr(%s, %s)",
                        left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kMax, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kMax, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

// ----- Max(expr, constant) -----

class MaxCstIntExpr : public BaseIntExpr {
 public:
  MaxCstIntExpr(Solver* const s, IntExpr* const e, int64 v)
      : BaseIntExpr(s), expr_(e), value_(v) {}
  virtual ~MaxCstIntExpr() {}
  virtual int64 Min() const {
    const int64 emin = expr_->Min();
    return std::max(emin, value_);
  }
  virtual void SetMin(int64 m) {
    if (value_ < m) {
      expr_->SetMin(m);
    }
  }
  virtual int64 Max() const {
    const int64 emax = expr_->Max();
    return std::max(emax, value_);
  }
  virtual void SetMax(int64 m) {
    if (m < value_) {
      solver()->Fail();
    }
    expr_->SetMax(m);
  }
  virtual bool Bound() const {
    return (expr_->Bound() || expr_->Max() <= value_);
  }
  virtual string name() const {
    return StringPrintf("MaxCstIntExpr(%s, %" GG_LL_FORMAT "d)",
                        expr_->name().c_str(), value_);
  }
  virtual string DebugString() const {
    return StringPrintf("MaxCstIntExpr(%s, %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), value_);
  }
  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kMax, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kMax, this);
  }

 private:
  IntExpr* const expr_;
  const int64 value_;
};

// ----- Convex Piecewise -----

// This class is a very simple convex piecewise linear function.  The
// argument of the function is the expression.  Between early_date and
// late_date, the value of the function is 0.  Before early date, it
// is affine and the cost is early_cost * (early_date - x). After
// late_date, the cost is late_cost * (x - late_date).

class SimpleConvexPiecewiseExpr : public BaseIntExpr {
 public:
  SimpleConvexPiecewiseExpr(Solver* const s, IntVar* const e,
                            int64 ec, int64 ed,
                            int64 ld, int64 lc)
      : BaseIntExpr(s),
    var_(e),
    early_cost_(ec),
    early_date_(ec == 0 ? kint64min : ed),
    late_date_(lc == 0 ? kint64max : ld),
    late_cost_(lc) {
        DCHECK_GE(ec, 0LL);
        DCHECK_GE(lc, 0LL);
        DCHECK_GE(ld, ed);

        // If the penalty is 0, we can push the "confort zone or zone
        // of no cost towards infinity.
  }

  virtual ~SimpleConvexPiecewiseExpr() {}

  virtual int64 Min() const {
    const int64 vmin = var_->Min();
    const int64 vmax = var_->Max();
    if (vmin >= late_date_) {
      return (vmin - late_date_) * late_cost_;
    } else if (vmax <= early_date_) {
      return (early_date_ - vmax) * early_cost_;
    } else {
      return 0LL;
    }
  }

  virtual void SetMin(int64 m) {
    if (m <= 0) {
      return;
    }
    const int64 vmin = var_->Min();
    const int64 vmax = var_->Max();
    const int64 rb = (late_cost_ == 0 ?
                      vmax :
                      late_date_ + PosIntDivUp(m , late_cost_) - 1);
    const int64 lb = (early_cost_ == 0 ?
                      vmin :
                      early_date_ - PosIntDivUp(m , early_cost_) + 1);
    var_->RemoveInterval(lb, rb);
  }

  virtual int64 Max() const {
    const int64 vmin = var_->Min();
    const int64 vmax = var_->Max();
    const int64 mr = vmax > late_date_ ? (vmax - late_date_) * late_cost_ : 0;
    const int64 ml = vmin < early_date_
        ? (early_date_ - vmin) * early_cost_
        : 0;
    return std::max(mr, ml);
  }

  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    if (late_cost_ != 0LL) {
      const int64 rb = late_date_ + PosIntDivDown(m, late_cost_);
      if (early_cost_ != 0LL) {
        const int64 lb = early_date_ - PosIntDivDown(m, early_cost_);
        var_->SetRange(lb, rb);
      } else {
        var_->SetMax(rb);
      }
    } else {
      if (early_cost_ != 0LL) {
        const int64 lb = early_date_ - PosIntDivDown(m, early_cost_);
        var_->SetMin(lb);
      }
    }
  }

  virtual string name() const {
    return StringPrintf(
        "ConvexPiecewiseExpr(%s, ec = %" GG_LL_FORMAT "d, ed = %"
        GG_LL_FORMAT "d, ld = %" GG_LL_FORMAT "d, lc = %" GG_LL_FORMAT "d)",
        var_->name().c_str(),
        early_cost_, early_date_, late_date_, late_cost_);
  }

  virtual string DebugString() const {
    return StringPrintf(
        "ConvexPiecewiseExpr(%s, ec = %" GG_LL_FORMAT "d, ed = %"
        GG_LL_FORMAT "d, ld = %" GG_LL_FORMAT "d, lc = %" GG_LL_FORMAT "d)",
        var_->DebugString().c_str(),
        early_cost_, early_date_, late_date_, late_cost_);
  }

  virtual void WhenRange(Demon* d) {
    var_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kConvexPiecewise, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitIntegerArgument(ModelVisitor::kEarlyCostArgument,
                                  early_cost_);
    visitor->VisitIntegerArgument(ModelVisitor::kEarlyDateArgument,
                                  early_date_);
    visitor->VisitIntegerArgument(ModelVisitor::kLateCostArgument,
                                  late_cost_);
    visitor->VisitIntegerArgument(ModelVisitor::kLateDateArgument,
                                  late_date_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kConvexPiecewise, this);
  }

 private:
  IntVar* const var_;
  const int64 early_cost_;
  const int64 early_date_;
  const int64 late_date_;
  const int64 late_cost_;
};

// ----- Semi Continuous -----

class SemiContinuousExpr : public BaseIntExpr {
 public:
  SemiContinuousExpr(Solver* const s, IntExpr* const e,
                     int64 fixed_charge, int64 step)
      : BaseIntExpr(s), expr_(e), fixed_charge_(fixed_charge), step_(step) {
        DCHECK_GE(fixed_charge, 0LL);
        DCHECK_GT(step, 0LL);
  }

  virtual ~SemiContinuousExpr() {}

  int64 Value(int64 x) const {
    if (x <= 0) {
      return 0;
    } else {
      return fixed_charge_ + x * step_;
    }
  }

  virtual int64 Min() const {
    return Value(expr_->Min());
  }

  virtual void SetMin(int64 m) {
    if (m >= fixed_charge_ + step_) {
      const int64 y = PosIntDivUp(m - fixed_charge_, step_);
      expr_->SetMin(y);
    } else if (m > 0) {
      expr_->SetMin(1);
    }
  }

  virtual int64 Max() const {
    return Value(expr_->Max());
  }

  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    if (m < fixed_charge_ + step_) {
      expr_->SetMax(0);
    } else {
      const int64 y = PosIntDivDown(m - fixed_charge_, step_);
      expr_->SetMax(y);
    }
  }

  virtual string name() const {
    return StringPrintf("SemiContinuous(%s, fixed_charge = %" GG_LL_FORMAT
                        "d, step = %" GG_LL_FORMAT "d)",
                        expr_->name().c_str(), fixed_charge_, step_);
  }

  virtual string DebugString() const {
    return StringPrintf("SemiContinuous(%s, fixed_charge = %" GG_LL_FORMAT
                        "d, step = %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), fixed_charge_, step_);
  }

  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kFixedChargeArgument,
                                  fixed_charge_);
    visitor->VisitIntegerArgument(ModelVisitor::kStepArgument, step_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
  }

 private:
  IntExpr* const expr_;
  const int64 fixed_charge_;
  const int64 step_;
};

class SemiContinuousStepOneExpr : public BaseIntExpr {
 public:
  SemiContinuousStepOneExpr(Solver* const s, IntExpr* const e,
                            int64 fixed_charge)
      : BaseIntExpr(s), expr_(e), fixed_charge_(fixed_charge) {
        DCHECK_GE(fixed_charge, 0LL);
  }

  virtual ~SemiContinuousStepOneExpr() {}

  int64 Value(int64 x) const {
    if (x <= 0) {
      return 0;
    } else {
      return fixed_charge_ + x;
    }
  }

  virtual int64 Min() const {
    return Value(expr_->Min());
  }

  virtual void SetMin(int64 m) {
    if (m >= fixed_charge_ + 1) {
      expr_->SetMin(m - fixed_charge_);
    } else if (m > 0) {
      expr_->SetMin(1);
    }
  }

  virtual int64 Max() const {
    return Value(expr_->Max());
  }

  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    if (m < fixed_charge_ + 1) {
      expr_->SetMax(0);
    } else {
      expr_->SetMax(m - fixed_charge_);
    }
  }

  virtual string name() const {
    return StringPrintf("SemiContinuousStepOne(%s, fixed_charge = %"
                        GG_LL_FORMAT "d)",
                        expr_->name().c_str(), fixed_charge_);
  }

  virtual string DebugString() const {
    return StringPrintf("SemiContinuousStepOne(%s, fixed_charge = %"
                        GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), fixed_charge_);
  }

  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kFixedChargeArgument,
                                  fixed_charge_);
    visitor->VisitIntegerArgument(ModelVisitor::kStepArgument, 1);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
  }

 private:
  IntExpr* const expr_;
  const int64 fixed_charge_;
};

class SemiContinuousStepZeroExpr : public BaseIntExpr {
 public:
  SemiContinuousStepZeroExpr(Solver* const s, IntExpr* const e,
                             int64 fixed_charge)
      : BaseIntExpr(s), expr_(e), fixed_charge_(fixed_charge) {
        DCHECK_GT(fixed_charge, 0LL);
  }

  virtual ~SemiContinuousStepZeroExpr() {}

  int64 Value(int64 x) const {
    if (x <= 0) {
      return 0;
    } else {
      return fixed_charge_;
    }
  }

  virtual int64 Min() const {
    return Value(expr_->Min());
  }

  virtual void SetMin(int64 m) {
    if (m >= fixed_charge_) {
      solver()->Fail();
    } else if (m > 0) {
      expr_->SetMin(1);
    }
  }

  virtual int64 Max() const {
    return Value(expr_->Max());
  }

  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    if (m < fixed_charge_) {
      expr_->SetMax(0);
    }
  }

  virtual string name() const {
    return StringPrintf("SemiContinuousStepZero(%s, fixed_charge = %"
        GG_LL_FORMAT "d)",
        expr_->name().c_str(), fixed_charge_);
  }

  virtual string DebugString() const {
    return StringPrintf("SemiContinuousStepZero(%s, fixed_charge = %"
                        GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), fixed_charge_);
  }

  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kFixedChargeArgument,
                                  fixed_charge_);
    visitor->VisitIntegerArgument(ModelVisitor::kStepArgument, 0);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
  }

 private:
  IntExpr* const expr_;
  const int64 fixed_charge_;
};

// This constraints links an expression and the variable it is casted into
class LinkExprAndVar : public CastConstraint {
 public:
  LinkExprAndVar(Solver* const s,
                 IntExpr* const expr,
                 IntVar* const var) : CastConstraint(s, var), expr_(expr) {}

  virtual ~LinkExprAndVar() {}

  virtual void Post() {
    Solver* const s = solver();
    Demon* d = s->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(d);
    target_var_->WhenRange(d);
  }

  virtual void InitialPropagate() {
    expr_->SetRange(target_var_->Min(), target_var_->Max());
    int64 l, u;
    expr_->Range(&l, &u);
    target_var_->SetRange(l, u);
  }

  virtual string DebugString() const {
    return StringPrintf("cast(%s, %s)",
                        expr_->DebugString().c_str(),
                        target_var_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kLinkExprVar, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kLinkExprVar, this);
  }

 private:
  IntExpr* const expr_;
};

// ----- This is a specialized case when the variable exact type is known -----
class LinkExprAndDomainIntVar : public CastConstraint {
 public:
  LinkExprAndDomainIntVar(Solver* const s,
                          IntExpr* const expr,
                          DomainIntVar* const var)
      : CastConstraint(s, var), expr_(expr), cached_min_(kint64min),
        cached_max_(kint64max), fail_stamp_(GG_ULONGLONG(0)) {}

  virtual ~LinkExprAndDomainIntVar() {}

  DomainIntVar* var() const {
    return reinterpret_cast<DomainIntVar*>(target_var_);
  }

  virtual void Post() {
    Solver* const s = solver();
    Demon* const d = s->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(d);
    Demon* const target_var_demon =
        MakeConstraintDemon0(solver(),
                             this,
                             &LinkExprAndDomainIntVar::Propagate,
                             "Propagate");
    target_var_->WhenRange(target_var_demon);
  }

  virtual void InitialPropagate() {
    expr_->SetRange(var()->min_.Value(), var()->max_.Value());
    expr_->Range(&cached_min_, &cached_max_);
    var()->DomainIntVar::SetRange(cached_min_, cached_max_);
  }

  void Propagate() {
    if (var()->min_.Value() > cached_min_ ||
        var()->max_.Value() < cached_max_ ||
        solver()->fail_stamp() != fail_stamp_) {
      InitialPropagate();
      fail_stamp_ = solver()->fail_stamp();
    }
  }

  virtual string DebugString() const {
    return StringPrintf("cast(%s, %s)",
                        expr_->DebugString().c_str(),
                        target_var_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kLinkExprVar, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kLinkExprVar, this);
  }

 private:
  IntExpr* const expr_;
  int64 cached_min_;
  int64 cached_max_;
  uint64 fail_stamp_;
};

// ----- Utilities -----

// Variable-based queue cleaner. It is used to put a domain int var in
// a clean state after a failure occuring during its process() method.
class VariableQueueCleaner : public Action {
 public:
  VariableQueueCleaner() : var_(NULL) {}

  virtual ~VariableQueueCleaner() {}

  virtual void Run(Solver* const solver) {
    DCHECK(var_ != NULL);
    var_->ClearInProcess();
  }

  void set_var(DomainIntVar* const var) { var_ = var; }

 private:
  DomainIntVar* var_;
};
}  //  namespace

Action* NewDomainIntVarCleaner() {
  return new VariableQueueCleaner;
}

void Solver::set_queue_cleaner_on_fail(IntVar* const var) {
  DCHECK_EQ(DOMAIN_INT_VAR, var->VarType());
  DomainIntVar* const dvar = reinterpret_cast<DomainIntVar*>(var);
  VariableQueueCleaner* const cleaner =
      reinterpret_cast<VariableQueueCleaner*>(variable_cleaner_.get());
  cleaner->set_var(dvar);
  set_queue_action_on_fail(cleaner);
}

void RestoreBoolValue(IntVar* const var) {
  DCHECK_EQ(BOOLEAN_VAR, var->VarType());
  BooleanVar* const boolean_var = reinterpret_cast<BooleanVar*>(var);
  boolean_var->RestoreValue();
}

// ----- API -----

IntVar* Solver::MakeIntVar(int64 min, int64 max, const string& name) {
  if (min == max) {
    return RevAlloc(new IntConst(this, min, name));
  }
  if (min == 0 && max == 1) {
    return RegisterIntVar(RevAlloc(new BooleanVar(this, name)));
  } else {
    return RegisterIntVar(RevAlloc(new DomainIntVar(this, min, max, name)));
  }
}

IntVar* Solver::MakeIntVar(int64 min, int64 max) {
  return MakeIntVar(min, max, "");
}

IntVar* Solver::MakeBoolVar(const string& name) {
  return RegisterIntVar(RevAlloc(new BooleanVar(this, name)));
}

IntVar* Solver::MakeBoolVar() {
  return RegisterIntVar(RevAlloc(new BooleanVar(this, "")));
}

IntVar* Solver::MakeIntVar(const std::vector<int64>& values, const string& name) {
  ConstIntArray domain(values);
  scoped_ptr<std::vector<int64> > sorted_values(
      domain.SortedCopyWithoutDuplicates(true));
  IntVar* const var =
      RegisterIntVar(RevAlloc(new DomainIntVar(this,
                                               *sorted_values.get(),
                                               name)));
  return var;
}

IntVar* Solver::MakeIntVar(const std::vector<int64>& values) {
  return MakeIntVar(values, "");
}

IntVar* Solver::MakeIntVar(const std::vector<int>& values, const string& name) {
  ConstIntArray domain(values);
  scoped_ptr<std::vector<int64> > sorted_values(
      domain.SortedCopyWithoutDuplicates(true));
  IntVar* const var =
      RegisterIntVar(RevAlloc(new DomainIntVar(this,
                                               *sorted_values.get(),
                                               name)));
  return var;
}

IntVar* Solver::MakeIntVar(const std::vector<int>& values) {
  return MakeIntVar(values, "");
}

IntVar* Solver::MakeIntConst(int64 val, const string& name) {
  // If IntConst is going to be named after its creation,
  // cp_share_int_consts should be set to false otherwise names can potentially
  // be overwritten.
  if (FLAGS_cp_share_int_consts &&
      name.empty() &&
      val >= MIN_CACHED_INT_CONST && val <= MAX_CACHED_INT_CONST) {
    return cached_constants_[val - MIN_CACHED_INT_CONST];
  }
  return RevAlloc(new IntConst(this, val, name));
}

IntVar* Solver::MakeIntConst(int64 val) {
  return MakeIntConst(val, "");
}

// ----- Int Var and associated methods -----

namespace {
string IndexedName(const string& prefix, int index, int max_index) {
#if 0
#if defined(_MSC_VER)
  const int digits = max_index > 0 ?
      static_cast<int>(log(1.0L * max_index) / log(10.0L)) + 1 :
      1;
#else
  const int digits = max_index > 0 ? static_cast<int>(log10(max_index)) + 1: 1;
#endif
  return StringPrintf("%s%0*d", prefix.c_str(), digits, index);
#else
  return StringPrintf("%s%d", prefix.c_str(), index);
#endif
}
}  // namespace

void Solver::MakeIntVarArray(int var_count,
                             int64 vmin,
                             int64 vmax,
                             const string& name,
                             std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeIntVar(vmin, vmax, IndexedName(name, i, var_count)));
  }
}

void Solver::MakeIntVarArray(int var_count,
                             int64 vmin,
                             int64 vmax,
                             std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeIntVar(vmin, vmax));
  }
}

IntVar** Solver::MakeIntVarArray(int var_count,
                                 int64 vmin,
                                 int64 vmax,
                                 const string& name) {
  IntVar** vars = new IntVar*[var_count];
  for (int i = 0; i < var_count; ++i) {
    vars[i] = MakeIntVar(vmin, vmax, IndexedName(name, i, var_count));
  }
  return vars;
}

void Solver::MakeBoolVarArray(int var_count,
                              const string& name,
                              std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeBoolVar(IndexedName(name, i, var_count)));
  }
}

void Solver::MakeBoolVarArray(int var_count, std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeBoolVar());
  }
}

IntVar** Solver::MakeBoolVarArray(int var_count, const string& name) {
  IntVar** vars = new IntVar*[var_count];
  for (int i = 0; i < var_count; ++i) {
    vars[i] = MakeBoolVar(IndexedName(name, i, var_count));
  }
  return vars;
}

void Solver::InitCachedIntConstants() {
  for (int i = MIN_CACHED_INT_CONST; i <= MAX_CACHED_INT_CONST; ++i) {
    cached_constants_[i - MIN_CACHED_INT_CONST] =
      RevAlloc(new IntConst(this, i, ""));  // note the empty name
  }
}

IntExpr* Solver::MakeSum(IntExpr* const l, IntExpr* const r) {
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (r->Bound()) {
    return MakeSum(l, r->Min());
  }
  if (l->Bound()) {
    return MakeSum(r, l->Min());
  }
  if (l == r) {
    return MakeProd(l, 2);
  }
  return RegisterIntExpr(RevAlloc(new PlusIntExpr(this, l, r)));
}

IntExpr* Solver::MakeSum(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  if (e->Bound()) {
    return MakeIntConst(e->Min() + v);
  }
  if (v == 0) {
    return e;
  }
  return RegisterIntExpr(RevAlloc(new PlusIntCstExpr(this, e, v)));
}
IntExpr* Solver::MakeDifference(IntExpr* const l, IntExpr* const r) {
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l->Bound()) {
    return MakeDifference(l->Min(), r);
  }
  if (r->Bound()) {
    return MakeSum(l, -r->Min());
  }
  return RegisterIntExpr(RevAlloc(new SubIntExpr(this, l, r)));
}

IntVar* Solver::MakeIsEqualVar(IntExpr* const v1, IntExpr* const v2) {
  CHECK_EQ(this, v1->solver());
  CHECK_EQ(this, v2->solver());
  if (v1->Bound()) {
    return MakeIsEqualCstVar(v2->Var(), v1->Min());
  } else if (v2->Bound()) {
    return MakeIsEqualCstVar(v1->Var(), v2->Min());
  }
  IntVar* const var1 = v1->Var();
  IntVar* const var2 = v2->Var();
  IntExpr* const cache = model_cache_->FindVarVarExpression(
      var1,
      var2,
      ModelCache::VAR_VAR_IS_EQUAL);
  if (cache != NULL) {
    return cache->Var();
  } else {
    string name1 = var1->name();
    if (name1.empty()) {
      name1 = var1->DebugString();
    }
    string name2 = var2->name();
    if (name2.empty()) {
      name2 = var2->DebugString();
    }
    IntVar* const boolvar = MakeIsEqualCstVar(MakeDifference(v1, v2)->Var(), 0);
    model_cache_->InsertVarVarExpression(
        boolvar,
        var1,
        var2,
        ModelCache::VAR_VAR_IS_EQUAL);
    return boolvar;
  }
}

Constraint* Solver::MakeIsEqualCt(IntExpr* const v1,
                                  IntExpr* const v2,
                                  IntVar* b) {
  CHECK_EQ(this, v1->solver());
  CHECK_EQ(this, v2->solver());
  if (v1->Bound()) {
    return MakeIsEqualCstCt(v2->Var(), v1->Min(), b);
  } else if (v2->Bound()) {
    return MakeIsEqualCstCt(v1->Var(), v2->Min(), b);
  }
  return MakeIsEqualCstCt(MakeDifference(v1, v2)->Var(), 0, b);
}

IntVar* Solver::MakeIsDifferentVar(IntExpr* const v1, IntExpr* const v2) {
  CHECK_EQ(this, v1->solver());
  CHECK_EQ(this, v2->solver());
  if (v1->Bound()) {
    return MakeIsDifferentCstVar(v2->Var(), v1->Min());
  } else if (v2->Bound()) {
    return MakeIsDifferentCstVar(v1->Var(), v2->Min());
  }
  IntVar* const var1 = v1->Var();
  IntVar* const var2 = v2->Var();
  IntExpr* const cache = model_cache_->FindVarVarExpression(
      var1,
      var2,
      ModelCache::VAR_VAR_IS_NOT_EQUAL);
  if (cache != NULL) {
    return cache->Var();
  } else {
    string name1 = var1->name();
    if (name1.empty()) {
      name1 = var1->DebugString();
    }
    string name2 = var2->name();
    if (name2.empty()) {
      name2 = var2->DebugString();
    }
    IntVar* const boolvar =
        MakeIsDifferentCstVar(MakeDifference(v1, v2)->Var(), 0);
    model_cache_->InsertVarVarExpression(
        boolvar,
        var1,
        var2,
        ModelCache::VAR_VAR_IS_NOT_EQUAL);
    return boolvar;
  }
}

Constraint* Solver::MakeIsDifferentCt(IntExpr* const v1,
                                      IntExpr* const v2,
                                      IntVar* b) {
  CHECK_EQ(this, v1->solver());
  CHECK_EQ(this, v2->solver());
  if (v1->Bound()) {
    return MakeIsDifferentCstCt(v2->Var(), v1->Min(), b);
  } else if (v2->Bound()) {
    return MakeIsDifferentCstCt(v1->Var(), v2->Min(), b);
  }
  return MakeIsDifferentCstCt(MakeDifference(v1, v2)->Var(), 0, b);
}

IntVar* Solver::MakeIsLessOrEqualVar(
    IntExpr* const left, IntExpr* const right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeIsGreaterOrEqualCstVar(right->Var(), left->Min());
  } else if (right->Bound()) {
    return MakeIsLessOrEqualCstVar(left->Var(), right->Min());
  }
  IntVar* const var1 = left->Var();
  IntVar* const var2 = right->Var();
  IntExpr* const cache = model_cache_->FindVarVarExpression(
      var1,
      var2,
      ModelCache::VAR_VAR_IS_LESS_OR_EQUAL);
  if (cache != NULL) {
    return cache->Var();
  } else {
    string name1 = var1->name();
    if (name1.empty()) {
      name1 = var1->DebugString();
    }
    string name2 = var2->name();
    if (name2.empty()) {
      name2 = var2->DebugString();
    }
    IntVar* const boolvar =
        MakeIsLessOrEqualCstVar(MakeDifference(left, right)->Var(), 0);
    model_cache_->InsertVarVarExpression(
        boolvar,
        var1,
        var2,
        ModelCache::VAR_VAR_IS_LESS_OR_EQUAL);
    return boolvar;
  }
}

Constraint* Solver::MakeIsLessOrEqualCt(
    IntExpr* const left, IntExpr* const right, IntVar* const b) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeIsGreaterOrEqualCstCt(right->Var(), left->Min(), b);
  } else if (right->Bound()) {
    return MakeIsLessOrEqualCstCt(left->Var(), right->Min(), b);
  }
  return MakeIsLessOrEqualCstCt(MakeDifference(left, right)->Var(), 0, b);
}

IntVar* Solver::MakeIsLessVar(
    IntExpr* const left, IntExpr* const right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeIsGreaterCstVar(right->Var(), left->Min());
  } else if (right->Bound()) {
    return MakeIsLessCstVar(left->Var(), right->Min());
  }
  IntVar* const var1 = left->Var();
  IntVar* const var2 = right->Var();
  IntExpr* const cache = model_cache_->FindVarVarExpression(
      var1,
      var2,
      ModelCache::VAR_VAR_IS_LESS);
  if (cache != NULL) {
    return cache->Var();
  } else {
    string name1 = var1->name();
    if (name1.empty()) {
      name1 = var1->DebugString();
    }
    string name2 = var2->name();
    if (name2.empty()) {
      name2 = var2->DebugString();
    }
    IntVar* const boolvar =
        MakeIsLessCstVar(MakeDifference(left, right)->Var(), 0);
    model_cache_->InsertVarVarExpression(
        boolvar,
        var1,
        var2,
        ModelCache::VAR_VAR_IS_LESS);
    return boolvar;
  }
}

Constraint* Solver::MakeIsLessCt(
    IntExpr* const left, IntExpr* const right, IntVar* const b) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeIsGreaterCstCt(right->Var(), left->Min(), b);
  } else if (right->Bound()) {
    return MakeIsLessCstCt(left->Var(), right->Min(), b);
  }
  return MakeIsLessCstCt(MakeDifference(left, right)->Var(), 0, b);
}

IntVar* Solver::MakeIsGreaterOrEqualVar(
    IntExpr* const left, IntExpr* const right) {
  return MakeIsLessOrEqualVar(right, left);
}

Constraint* Solver::MakeIsGreaterOrEqualCt(
    IntExpr* const left, IntExpr* const right, IntVar* const b) {

  return MakeIsLessOrEqualCt(right, left, b);
}

IntVar* Solver::MakeIsGreaterVar(
    IntExpr* const left, IntExpr* const right) {
  return MakeIsLessVar(right, left);
}

Constraint* Solver::MakeIsGreaterCt(
    IntExpr* const left, IntExpr* const right, IntVar* const b) {
  return MakeIsLessCt(right, left, b);
}

// warning: this is 'v - e'.
IntExpr* Solver::MakeDifference(int64 v, IntExpr* const e) {
  CHECK_EQ(this, e->solver());
  if (e->Bound()) {
    return MakeIntConst(v - e->Min());
  }
  if (v == 0) {
    return MakeOpposite(e);
  }
  return RegisterIntExpr(RevAlloc(new SubIntCstExpr(this, e, v)));
}

IntExpr* Solver::MakeOpposite(IntExpr* const e) {
  CHECK_EQ(this, e->solver());
  if (e->Bound()) {
    return MakeIntConst(-e->Min());
  }
  if (e->IsVar()) {
    return RegisterIntVar(RevAlloc(new OppIntExpr(this, e))->Var());
  } else {
    return RegisterIntExpr(RevAlloc(new OppIntExpr(this, e)));
  }
}

IntExpr* Solver::MakeProd(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  IntExpr* result;
  if (e->Bound()) {
    return MakeIntConst(v * e->Min());
  } else if (v == 1) {
    return e;
  } else if (v == -1) {
    return MakeOpposite(e);
  } else if (v > 0) {
    result = RegisterIntExpr(RevAlloc(new TimesIntPosCstExpr(this, e, v)));
  } else if (v == 0) {
    result = MakeIntConst(0);
  } else {
    result = RegisterIntExpr(RevAlloc(new TimesIntNegCstExpr(this, e, v)));
  }
  if (e->IsVar() && !FLAGS_cp_disable_expression_optimization) {
    result = result->Var();
  }
  return result;
}

IntExpr* Solver::MakeProd(IntExpr* const l, IntExpr* const r) {
  if (l->Bound()) {
    return MakeProd(r, l->Min());
  }
  if (r->Bound()) {
    return MakeProd(l, r->Min());
  }
  if (l == r) {
    return MakeSquare(l);
  }
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l->IsVar() &&
      reinterpret_cast<IntVar*>(l)->VarType() == BOOLEAN_VAR) {
    if (r->Min() >= 0) {
      return RegisterIntExpr(RevAlloc(
          new TimesBooleanPosIntExpr(this,
                                     reinterpret_cast<BooleanVar*>(l), r)));
    } else {
      return RegisterIntExpr(RevAlloc(
          new TimesBooleanIntExpr(this, reinterpret_cast<BooleanVar*>(l), r)));
    }
  }
  if (r->IsVar() &&
      reinterpret_cast<IntVar*>(r)->VarType() == BOOLEAN_VAR) {
    if (l->Min() >= 0) {
      return RegisterIntExpr(RevAlloc(
          new TimesBooleanPosIntExpr(this,
                                     reinterpret_cast<BooleanVar*>(r), l)));
    } else {
      return RegisterIntExpr(RevAlloc(
          new TimesBooleanIntExpr(this, reinterpret_cast<BooleanVar*>(r), l)));
    }
  }
  if (l->Min() >= 0 && r->Min() >= 0) {
    return RegisterIntExpr(RevAlloc(new TimesIntPosExpr(this, l, r)));
  } else {
    return RegisterIntExpr(RevAlloc(new TimesIntExpr(this, l, r)));
  }
}

IntExpr* Solver::MakeDiv(IntExpr* const numerator, IntExpr* const denominator) {
  // Both numerator and denominator are positive.
  // Denominator needs to be != 0.
  AddConstraint(MakeGreater(denominator, 0));
  IntVar* const result = MakeIntVar(0, numerator->Max());
  IntExpr* const product = MakeProd(denominator, result);
  // The integer division result = numerator / denominator means
  // numerator = result * denominator + quotient, with quotient < denominator.
  // This translates into:
  //     numerator >= denominator * result
  AddConstraint(MakeGreaterOrEqual(numerator->Var(), product->Var()));
  //     numerator < denominator * result + denominator
  IntExpr* const product_up = RegisterIntExpr(MakeSum(product, denominator));
  AddConstraint(MakeLess(numerator->Var(), product_up->Var()));
  return result;
}

IntExpr* Solver::MakeDiv(IntExpr* const e, int64 v) {
  CHECK(e != NULL);
  CHECK_EQ(this, e->solver());
  if (e->Bound()) {
    return MakeIntConst(PosIntDivDown(e->Min(), v));
  } else if (v == 1) {
    return e;
  } else if (v == -1) {
    return MakeOpposite(e);
  } else if (v > 0) {
    return RegisterIntExpr(RevAlloc(new DivIntPosCstExpr(this, e, v)));
  } else if (v == 0) {
    LOG(FATAL) << "Cannot divide by 0";
    return NULL;
  } else {
    return RegisterIntExpr(
        MakeOpposite(RevAlloc(new DivIntPosCstExpr(this, e, -v))));
    // TODO(user) : implement special case.
  }
}

IntExpr* Solver::MakeAbs(IntExpr* const e) {
  CHECK_EQ(this, e->solver());
  if (e->Min() >= 0) {
    return e;
  } else if (e->Max() <= 0) {
    return MakeOpposite(e);
  }
  return RegisterIntExpr(RevAlloc(new IntAbs(this, e)));
}

IntExpr* Solver::MakeSquare(IntExpr* const e) {
  CHECK_EQ(this, e->solver());
  if (e->Bound()) {
    const int64 v = e->Min();
    return MakeIntConst(v * v);
  }
  if (e->Min() >= 0) {
    return RegisterIntExpr(RevAlloc(new PosIntSquare(this, e)));
  }
  return RegisterIntExpr(RevAlloc(new IntSquare(this, e)));
}

IntExpr* Solver::MakeMin(IntExpr* const l, IntExpr* const r) {
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l->Bound()) {
    return MakeMin(r, l->Min());
  }
  if (r->Bound()) {
    return MakeMin(l, r->Min());
  }
  if (l->Min() > r->Max()) {
    return r;
  }
  if (r->Min() > l->Max()) {
    return l;
  }
  return RegisterIntExpr(RevAlloc(new MinIntExpr(this, l, r)));
}

IntExpr* Solver::MakeMin(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  if (v < e->Min()) {
    return MakeIntConst(v);
  }
  if (e->Bound()) {
    return MakeIntConst(std::min(e->Min(), v));
  }
  if (e->Max() < v) {
    return e;
  }
  return RegisterIntExpr(RevAlloc(new MinCstIntExpr(this, e, v)));
}

IntExpr* Solver::MakeMin(IntExpr* const e, int v) {
  return MakeMin(e, static_cast<int64>(v));
}

IntExpr* Solver::MakeMax(IntExpr* const l, IntExpr* const r) {
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l->Bound()) {
    return MakeMax(r, l->Min());
  }
  if (r->Bound()) {
    return MakeMax(l, r->Min());
  }
  if (l->Min() > r->Max()) {
    return l;
  }
  if (r->Min() > l->Max()) {
    return r;
  }
  return RegisterIntExpr(RevAlloc(new MaxIntExpr(this, l, r)));
}

IntExpr* Solver::MakeMax(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  if (e->Bound()) {
    return MakeIntConst(std::max(e->Min(), v));
  }
  if (v < e->Min()) {
    return e;
  }
  if (e->Max() < v) {
    return MakeIntConst(v);
  }
  return RegisterIntExpr(RevAlloc(new MaxCstIntExpr(this, e, v)));
}

IntExpr* Solver::MakeMax(IntExpr* const e, int v) {
  return MakeMax(e, static_cast<int64>(v));
}

IntExpr* Solver::MakeConvexPiecewiseExpr(IntVar* e,
                                         int64 early_cost, int64 early_date,
                                         int64 late_date, int64 late_cost) {
  return RegisterIntExpr(RevAlloc(
      new SimpleConvexPiecewiseExpr(this, e,
                                    early_cost, early_date,
                                    late_date, late_cost)));
}

IntExpr* Solver::MakeSemiContinuousExpr(IntExpr* const e,
                                        int64 fixed_charge,
                                        int64 step) {
  if (step == 0) {
    if (fixed_charge == 0) {
      return MakeIntConst(0LL);
    } else {
      return RegisterIntExpr(RevAlloc(
          new SemiContinuousStepZeroExpr(this, e, fixed_charge)));
    }
  } else if (step == 1) {
    return RegisterIntExpr(RevAlloc(
        new SemiContinuousStepOneExpr(this, e, fixed_charge)));
  } else {
    return RegisterIntExpr(RevAlloc(
        new SemiContinuousExpr(this, e, fixed_charge, step)));
  }
  // TODO(user) : benchmark with virtualization of
  // PosIntDivDown and PosIntDivUp - or function pointers.
}

// --------- IntVar ---------

int IntVar::VarType() const {
  return UNSPECIFIED;
}

void IntVar::RemoveValues(const int64* const values, int size) {
  DCHECK_GE(size, 0);
  for (int i = 0; i < size; ++i) {
    RemoveValue(values[i]);
  }
}

void IntVar::Accept(ModelVisitor* const visitor) const {
  const IntExpr* casted = NULL;
  const Solver::IntegerCastInfo* const cast_info =
      FindOrNull(solver()->cast_information_, this);
  if (cast_info != NULL) {
    casted = cast_info->expression;
  }
  visitor->VisitIntegerVariable(this, casted);
}

void IntVar::SetValues(const int64* const values, int size) {
  // TODO(user): reimplement all this!!
  // TODO(user): This code leaks if the array is not sorted.
  const int64* const new_array = IsArrayActuallySorted(values, size) ?
      values :
      NewUniqueSortedArray(values, &size);
  const int64 vmin = Min();
  const int64 vmax = Max();
  const int64* first_pos = new_array;
  const int64* last_pos = first_pos + size - 1;
  if (*first_pos > vmax || *last_pos < vmin) {
    solver()->Fail();
  }
  // TODO(user) : We could find the first position >= vmin by dichotomy.
  while (first_pos <= last_pos &&
         (*first_pos < vmin || !Contains(*first_pos))) {
    if (*first_pos > vmax) {
      solver()->Fail();
    }
    first_pos++;
  }
  if (first_pos > last_pos) {
    solver()->Fail();
  }
  while (last_pos >= first_pos &&
         (*last_pos > vmax ||
          !Contains(*last_pos))) {
    last_pos--;
  }
  DCHECK_GE(last_pos, first_pos);
  SetRange(*first_pos, *last_pos);
  while (first_pos < last_pos) {
    const int64 start = (*first_pos) + 1;
    const int64 end = *(first_pos + 1) - 1;
    if (start <= end) {
      RemoveInterval(start, end);
    }
    first_pos++;
  }
}

// ---------- BaseIntExpr ---------

void LinkVarExpr(Solver* const s, IntExpr* const expr, IntVar* const var) {
  if (!var->Bound()) {
    if (var->VarType() == DOMAIN_INT_VAR) {
      DomainIntVar* dvar = reinterpret_cast<DomainIntVar*>(var);
      s->AddCastConstraint(
          s->RevAlloc(new LinkExprAndDomainIntVar(s, expr, dvar)), dvar, expr);
    } else {
      s->AddCastConstraint(
          s->RevAlloc(new LinkExprAndVar(s, expr, var)), var, expr);
    }
  }
}

IntVar* BaseIntExpr::Var() {
  if (var_ == NULL) {
    solver()->SaveValue(reinterpret_cast<void**>(&var_));
    var_ = CastToVar();
  }
  return var_;
}

IntVar* BaseIntExpr::CastToVar() {
  int64 vmin, vmax;
  Range(&vmin, &vmax);
  IntVar* const var = solver()->MakeIntVar(vmin, vmax);
  LinkVarExpr(solver(), this, var);
  return var;
}
}   // namespace operations_research
