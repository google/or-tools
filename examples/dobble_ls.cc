// Copyright 2010 Google
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

// Dobble Generation problem:
//  - We have 57 cards
//  - 57 symbols
//  - 8 symbols per card
// We want to assign symbols per card such that any two cards have exactly
// one symbol in common.

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/concise_iterator.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/bitset.h"
#include "base/random.h"

DEFINE_int32(lns_size, 10, "Size of the lns fragment.");
DEFINE_int32(lns_limit, 5, "Limit the number of failures of the lns loop.");
DEFINE_int32(lns_seed, 1, "Seed for the LNS random number generator.");
DEFINE_int32(fail_limit, 50000, "Fail limit for the global search.");

namespace operations_research {
class IntersectionCount : public Constraint {
 public:
  IntersectionCount(Solver* const solver,
                    const vector<IntVar*>& vars1,
                    const vector<IntVar*>& vars2,
                    IntVar* const count_var,
                    int count)
      : Constraint(solver),
        vars1_(new IntVar*[vars1.size()]),
        vars2_(new IntVar*[vars2.size()]),
        size_(vars1.size()),
        count_var_(count_var),
        count_(count) {
    CHECK_EQ(vars1.size(), vars2.size());
    memcpy(vars1_.get(), vars1.data(), vars1.size() * sizeof(vars1.data()));
    memcpy(vars2_.get(), vars2.data(), vars2.size() * sizeof(vars2.data()));
    for (int i = 0; i < size_; ++i) {
      CHECK_GE(vars1_[i]->Min(), 0);
      CHECK_GE(vars2_[i]->Min(), 0);
      CHECK_LE(vars1_[i]->Max(), 1);
      CHECK_LE(vars2_[i]->Max(), 1);
    }
  }

  virtual void Post() {
    Demon* const delayed =
        solver()->MakeDelayedConstraintInitialPropagateCallback(this);
    for (int i = 0; i < size_; ++i) {
      vars1_[i]->WhenBound(delayed);
      vars2_[i]->WhenBound(delayed);
    }
  }

  virtual void InitialPropagate() {
    int possible = 0;
    int sure = 0;
    int unbounded = 0;
    for (int i = 0; i < size_; ++i) {
      if (vars1_[i]->Min() == 1 && vars2_[i]->Min() == 1) {
        sure++;
      }
      if (vars1_[i]->Max() == 1 && vars2_[i]->Max() == 1) {
        possible++;
      }
      if (!vars1_[i]->Bound() || !vars2_[i]->Bound()) {
        unbounded++;
      }
    }
    count_var_->SetRange(sure, possible);
    if (unbounded > 0) {
      if (count_var_->Max() == sure) {
        for (int i = 0; i < size_; ++i) {
          if (vars1_[i]->Min() == 1 && vars2_[i]->Min() == 0) {
            vars2_[i]->SetValue(0);
          } else if (vars2_[i]->Min() == 1 && vars1_[i]->Min() == 0) {
            vars1_[i]->SetValue(0);
          }
        }
      } else if (count_var_->Min() == possible) {
        for (int i = 0; i < size_; ++i) {
          if (vars1_[i]->Max() == 1 && vars2_[i]->Max() == 1) {
            vars1_[i]->SetValue(1);
            vars2_[i]->SetValue(1);
          }
        }
      }
    }
  }
 private:
  scoped_array<IntVar*> vars1_;
  scoped_array<IntVar*> vars2_;
  const int size_;
  IntVar* const count_var_;
  const int count_;
};

IntVar* AddIntersectionVar(Solver* const solver,
                           const vector<IntVar*>& vars1,
                           const vector<IntVar*>& vars2,
                           int count) {
  IntVar* const cardinality = solver->MakeIntVar(0, count);
  solver->AddConstraint(solver->RevAlloc(new IntersectionCount(solver,
                                                               vars1,
                                                               vars2,
                                                               cardinality,
                                                               count)));
  return solver->MakeAbs(solver->MakeSum(cardinality, -1))->Var();
}

class CardLns : public BaseLNS {
 public:
  CardLns(const IntVar* const* vars,
          int size,
          int fragment_size,
          int num_cards,
          int num_symbols)
      : BaseLNS(vars, size),
        rand_(FLAGS_lns_seed),
        fragment_size_(fragment_size),
        num_cards_(num_cards),
        num_symbols_(num_symbols) {}

  virtual ~CardLns() {}

  virtual bool NextFragment(vector<int>* fragment) {
    if (rand_.Uniform(2)) {  // Release Cards.
      for (int i = 0; i < fragment_size_; ++i) {
        const int card_index = rand_.Uniform(num_cards_);
        for (int symbol_index = 0;
             symbol_index < num_symbols_;
             ++symbol_index) {
          fragment->push_back(card_index * num_symbols_ + symbol_index);
        }
      }
    } else {  // Release Symbols.
      for (int i = 0; i < fragment_size_; ++i) {
        const int symbol_index = rand_.Uniform(num_symbols_);
        for (int card_index = 0; card_index < num_cards_; ++card_index) {
          fragment->push_back(card_index * num_symbols_ + symbol_index);
        }
      }
    }
    return true;
  }
 private:
  ACMRandom rand_;
  const int fragment_size_;
  const int num_cards_;
  const int num_symbols_;
};

class CrossLns : public BaseLNS {
 public:
  CrossLns(const IntVar* const* vars,
           int size,
           int fragment_size,
           int num_cards,
           int num_symbols,
           int num_symbols_per_card)
      : BaseLNS(vars, size),
        rand_(FLAGS_lns_seed),
        fragment_size_(fragment_size),
        num_cards_(num_cards),
        num_symbols_(num_symbols),
        num_symbols_per_card_(num_symbols_per_card),
        symbols_per_card_(num_cards) {
    for (int card = 0; card < num_cards_; ++card) {
      symbols_per_card_[card].resize(num_symbols_per_card_, -1);
    }
  }

  virtual ~CrossLns() {}

  virtual void InitFragments() {
    for (int card = 0; card < num_cards_; ++card) {
      int found = 0;
      for (int symbol = 0; symbol < num_symbols_; ++symbol) {
        if (Value(Index(card, symbol)) == 1) {
          symbols_per_card_[card][found++] = symbol;
        }
      }
      DCHECK_EQ(num_symbols_per_card_, found);
    }
  }

  virtual bool NextFragment(vector<int>* fragment) {
    hash_set<int> cards_to_release;
    const int max_cards = max(fragment_size_, num_cards_ / 2);
    hash_set<int> symbols_to_release;
    while (cards_to_release.size() < max_cards) {
      const int card = rand_.Uniform(num_cards_);
      const int symbol =
          symbols_per_card_[card][rand_.Uniform(num_symbols_per_card_)];
      cards_to_release.insert(card);
      symbols_to_release.insert(symbol);
    }

    for (ConstIter<hash_set<int> > card_it(cards_to_release);
         !card_it.at_end();
         ++card_it) {
      for (ConstIter<hash_set<int> > symbol_it(symbols_to_release);
           !symbol_it.at_end();
           ++symbol_it) {
        fragment->push_back(Index(*card_it, *symbol_it));
      }
    }
    return true;
  }
 private:
  int Index(int card, int symbol) {
    return card * num_symbols_ + symbol;
  }

  ACMRandom rand_;
  const int fragment_size_;
  const int num_cards_;
  const int num_symbols_;
  const int num_symbols_per_card_;
  vector<vector<int> > symbols_per_card_;
};

class SwitchSymbols : public IntVarLocalSearchOperator {
 public:
  SwitchSymbols(const IntVar* const* vars,
                int size,
                int num_cards,
                int num_symbols,
                int num_symbols_per_card)
      : IntVarLocalSearchOperator(vars, size),
        num_cards_(num_cards),
        num_symbols_(num_symbols),
        num_symbols_per_card_(num_symbols_per_card),
        current_card1_(-1),
        current_card2_(-1),
        current_symbol1_(-1),
        current_symbol2_(-1),
        symbols_per_card_(num_cards) {
    for (int card = 0; card < num_cards; ++card) {
      symbols_per_card_[card].resize(num_symbols_per_card, -1);
    }
  }

  virtual ~SwitchSymbols() {}

  virtual bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) {
    CHECK_NOTNULL(delta);
    while (true) {
      RevertChanges(true);
      if (!Increment()) {
        VLOG(1) << "finished nhood";
        return false;
      }
      const int symbol1 = symbols_per_card_[current_card1_][current_symbol1_];
      const int symbol2 = symbols_per_card_[current_card2_][current_symbol2_];
      DCHECK(Value(Index(current_card1_, symbol1)));
      DCHECK(Value(Index(current_card2_, symbol2)));
      if (Value(Index(current_card1_, symbol2)) ||
          Value(Index(current_card2_, symbol1))) {
        VLOG(1) << "Will overwrite symbol, continuing";
        continue;
      }
      SetValue(Index(current_card1_, symbol1), 0);
      SetValue(Index(current_card2_, symbol2), 0);
      SetValue(Index(current_card1_, symbol2), 1);
      SetValue(Index(current_card2_, symbol1), 1);
      if (ApplyChanges(delta, deltadelta)) {
        VLOG(1) << "Delta = " << delta->DebugString();
        return true;
      }
    }
    return false;
  }
 protected:
  virtual void OnStart() {
    VLOG(1) << "start nhood";
    for (int card = 0; card < num_cards_; ++card) {
      int found = 0;
      for (int symbol = 0; symbol < num_symbols_; ++symbol) {
        if (Value(Index(card, symbol)) == 1) {
          symbols_per_card_[card][found++] = symbol;
        }
      }
      DCHECK_EQ(num_symbols_per_card_, found);
    }
    current_card1_ = 0;
    current_card2_ = 1;
    current_symbol1_ = 0;
    current_symbol2_ = -1;
  }
 private:
  int Index(int card, int symbol) {
    return card * num_symbols_ + symbol;
  }

  bool Increment() {
    current_symbol2_++;
    if (current_symbol2_ == num_symbols_per_card_) {
      current_symbol2_ = 0;
      current_symbol1_++;
      if (current_symbol1_ == num_symbols_per_card_) {
        current_symbol1_ = 0;
        current_card2_++;
        if (current_card2_ == num_cards_) {
          current_card1_++;
          current_card2_ = current_card1_ + 1;
        }
      }
    }
    return current_card1_ < num_cards_ - 1;
  }

  const int num_cards_;
  const int num_symbols_;
  const int num_symbols_per_card_;
  int current_card1_;
  int current_card2_;
  int current_symbol1_;
  int current_symbol2_;
  vector<vector<int> > symbols_per_card_;
};

class CycleSymbols : public IntVarLocalSearchOperator {
 public:
  CycleSymbols(const IntVar* const* vars,
               int size,
               int num_cards,
               int num_symbols,
               int num_symbols_per_card)
      : IntVarLocalSearchOperator(vars, size),
        num_cards_(num_cards),
        num_symbols_(num_symbols),
        num_symbols_per_card_(num_symbols_per_card),
        current_card1_(-1),
        current_card2_(-1),
        current_symbol1_(-1),
        current_symbol2_(-1),
        symbols_per_card_(num_cards) {
    for (int card = 0; card < num_cards; ++card) {
      symbols_per_card_[card].resize(num_symbols_per_card, -1);
    }
    InitTriples();
  }

  virtual ~CycleSymbols() {}

  virtual bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) {
    CHECK_NOTNULL(delta);
    while (true) {
      RevertChanges(true);
      if (!Increment()) {
        VLOG(1) << "finished nhood";
        return false;
      }
      const int symbol1 = symbols_per_card_[current_card1_][current_symbol1_];
      const int symbol2 = symbols_per_card_[current_card2_][current_symbol2_];
      const int symbol3 = symbols_per_card_[current_card3_][current_symbol3_];
      DCHECK(Value(Index(current_card1_, symbol1)));
      DCHECK(Value(Index(current_card2_, symbol2)));
      DCHECK(Value(Index(current_card3_, symbol3)));
      if (Value(Index(current_card1_, symbol2)) ||
          Value(Index(current_card2_, symbol3)) ||
          Value(Index(current_card3_, symbol1))) {
        VLOG(1) << "Will overwrite symbol, continuing";
        continue;
      }
      SetValue(Index(current_card1_, symbol1), 0);
      SetValue(Index(current_card2_, symbol2), 0);
      SetValue(Index(current_card3_, symbol3), 0);
      SetValue(Index(current_card1_, symbol2), 1);
      SetValue(Index(current_card2_, symbol3), 1);
      SetValue(Index(current_card3_, symbol1), 1);
      if (ApplyChanges(delta, deltadelta)) {
        VLOG(1) << "Delta = " << delta->DebugString();
        return true;
      }
    }
    return false;
  }
 protected:
  virtual void OnStart() {
    VLOG(1) << "start nhood";
    for (int card = 0; card < num_cards_; ++card) {
      int found = 0;
      for (int symbol = 0; symbol < num_symbols_; ++symbol) {
        if (Value(Index(card, symbol)) == 1) {
          symbols_per_card_[card][found++] = symbol;
        }
      }
      DCHECK_EQ(num_symbols_per_card_, found);
    }
    card_triple_index_ = 0;
    current_card1_ = card_triples_[0].card1;
    current_card2_ = card_triples_[0].card2;
    current_card3_ = card_triples_[0].card3;
    current_symbol1_ = 0;
    current_symbol2_ = 0;
    current_symbol3_ = -1;
  }
 private:
  struct Triple {
    Triple(int c1, int c2, int c3) : card1(c1), card2(c2), card3(c3) {}
    int card1;
    int card2;
    int card3;
  };

  void InitTriples() {
    for (int c1 = 0; c1 < num_cards_; ++c1) {
      for (int c2 = 0; c2 < num_cards_; ++c2) {
        for (int c3 = 0; c3 < num_cards_; ++c3) {
          if (c1 != c2 && c1 != c3 && c2 != c3) {
            card_triples_.push_back(Triple(c1, c2, c3));
          }
        }
      }
    }
  }

  int Index(int card, int symbol) {
    return card * num_symbols_ + symbol;
  }

  void NextCardTriple() {
    card_triple_index_++;
    if (card_triple_index_ < card_triples_.size()) {
      const Triple& triple = card_triples_[card_triple_index_];
      current_card1_ = triple.card1;
      current_card2_ = triple.card2;
      current_card3_ = triple.card3;
    }
  }

  bool Increment() {
    current_symbol3_++;
    if (current_symbol3_ == num_symbols_per_card_) {
      current_symbol3_ = 0;
      current_symbol2_++;
      if (current_symbol2_ == num_symbols_per_card_) {
        current_symbol2_ = 0;
        current_symbol1_++;
        if (current_symbol1_ == num_symbols_per_card_) {
          current_symbol1_ = 0;
          NextCardTriple();
        }
      }
    }
    return card_triple_index_ < card_triples_.size();
  }

  const int num_cards_;
  const int num_symbols_;
  const int num_symbols_per_card_;
  int current_card1_;
  int current_card2_;
  int current_card3_;
  int current_symbol1_;
  int current_symbol2_;
  int current_symbol3_;
  vector<vector<int> > symbols_per_card_;
  int card_triple_index_;
  vector<Triple> card_triples_;
};

class CycleNeighborhood : public IntVarLocalSearchOperator {
 public:
  CycleNeighborhood(const IntVar* const* vars,
                    int size,
                    int max_size,
                    int num_cards,
                    int num_symbols,
                    int num_symbols_per_card)
      : IntVarLocalSearchOperator(vars, size),
        rand_(FLAGS_lns_seed),
        max_size_(max_size),
        num_cards_(num_cards),
        num_symbols_(num_symbols),
        num_symbols_per_card_(num_symbols_per_card),
        symbols_per_card_(num_cards) {
    for (int card = 0; card < num_cards_; ++card) {
      symbols_per_card_[card].resize(num_symbols_per_card_, -1);
    }
  }

  virtual ~CycleNeighborhood() {}

  virtual bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) {
    CHECK_NOTNULL(delta);
    while (true) {
      RevertChanges(true);

      const int num_cards_to_release = rand_.Uniform(max_size_ - 3) + 3;
      hash_set<int> cards_set;
      vector<ToSwap> to_swap;
      hash_set<int> symbols_to_release;
      while (cards_set.size() < num_cards_to_release) {
        const int card = rand_.Uniform(num_cards_);
        if (ContainsKey(cards_set, card)) {
          continue;
        }
        while (true) {
          const int symbol =
              symbols_per_card_[card][rand_.Uniform(num_symbols_per_card_)];
          if (!ContainsKey(symbols_to_release, symbol)) {
            to_swap.push_back(ToSwap(card, symbol));
            symbols_to_release.insert(symbol);
            break;
          }
        }
      }

      std::random_shuffle(to_swap.begin(), to_swap.end());
      for (int i = 0; i < num_cards_to_release; ++i) {
        SetValue(Index(to_swap[i].card, to_swap[i].symbol), 0);
        SetValue(Index(to_swap[i].card,
                       to_swap[(i + 1) % num_cards_to_release].symbol), 1);
      }

      if (ApplyChanges(delta, deltadelta)) {
        VLOG(1) << "Delta = " << delta->DebugString();
        return true;
      }
    }
    return false;
  }
 protected:
  virtual void OnStart() {
    for (int card = 0; card < num_cards_; ++card) {
      int found = 0;
      for (int symbol = 0; symbol < num_symbols_; ++symbol) {
        if (Value(Index(card, symbol)) == 1) {
          symbols_per_card_[card][found++] = symbol;
        }
      }
      DCHECK_EQ(num_symbols_per_card_, found);
    }
  }

 private:
  struct ToSwap {
    ToSwap(int c, int s) : card(c), symbol(s) {}
    int card;
    int symbol;
  };

  int Index(int card, int symbol) {
    return card * num_symbols_ + symbol;
  }

  ACMRandom rand_;
  const int max_size_;
  const int num_cards_;
  const int num_symbols_;
  const int num_symbols_per_card_;
  vector<vector<int> > symbols_per_card_;
};


class DobbleFilter : public IntVarLocalSearchFilter {
 public:
  DobbleFilter(const IntVar* const* vars,
               int size,
               int num_cards,
               int num_symbols,
               int num_symbols_per_card)
      : IntVarLocalSearchFilter(vars, size),
        num_cards_(num_cards),
        num_symbols_(num_symbols),
        num_symbols_per_card_(num_symbols_per_card),
        cards_(num_cards, 0ULL),
        costs_(num_cards_) {
    for (int card = 0; card < num_cards_; ++card) {
      costs_[card].resize(num_cards_, 0);
    }
  }

  virtual void OnSynchronize() {
    memset(cards_.data(), 0, num_cards_ * sizeof(*cards_.data()));
    for (int card = 0; card < num_cards_; ++card) {
      for (int symbol = 0; symbol < num_symbols_; ++symbol) {
        if (Value(card * num_symbols_ + symbol)) {
          SetBit64(&cards_[card], symbol);
        }
      }
    }
    for (int card1 = 0; card1 < num_cards_; ++card1) {
      for (int card2 = 0; card2 < num_cards_; ++card2) {
        costs_[card1][card2] =
            Cost(BitCount64(cards_[card1] & cards_[card2]));
      }
    }
    DCHECK(CheckCards());
  }

  virtual bool Accept(const Assignment* delta, const Assignment* deltadelta) {
    const Assignment::IntContainer& solution_delta = delta->IntVarContainer();
    const int solution_delta_size = solution_delta.Size();

    // First we check we are not using LNS.
    for (int i = 0; i < solution_delta_size; ++i) {
      const IntVarElement& element = solution_delta.Element(i);
      if (!element.Activated()) {
        // Doodle filters are not robust to LNS, so skip filters when
        // some elements are not activated.
        VLOG(1) << "LNS";
        return true;
      }
    }
    VLOG(1) << "No LNS, size = " << solution_delta_size;
    backtrack_.clear();
    hash_set<int> touched_cards;
    for (int index = 0; index < solution_delta_size; ++index) {
      int64 touched_var = -1;
      FindIndex(solution_delta.Element(index).Var(), &touched_var);
      const int card = touched_var / num_symbols_;
      const int symbol = touched_var % num_symbols_;
      const int new_value = solution_delta.Element(index).Value();
      if (!ContainsKey(touched_cards, card)) {
        Save(card);
        touched_cards.insert(card);
      }
      if (new_value) {
        SetBit64(&cards_[card], symbol);
      } else {
        ClearBit64(&cards_[card], symbol);
      }
    }

    if (!CheckCards()) {
      Backtrack();
      DCHECK(CheckCards());
      VLOG(1) << "reject by size";
      return false;
    }

    hash_set<int> treated_cards;
    int cost_delta = 0;
    for (ConstIter<hash_set<int> > card_it(touched_cards);
         !card_it.at_end();
         ++card_it) {
      treated_cards.insert(*card_it);
      const uint64 bitset = cards_[*card_it];
      const vector<int>& row_cost = costs_[*card_it];
      for (int card = 0; card < num_cards_; ++card) {
        if (!ContainsKey(treated_cards, card)) {
          cost_delta +=
              Cost(BitCount64(bitset & cards_[card])) - row_cost[card];
        }
      }
    }
    Backtrack();
    if (cost_delta >= 0) {
      VLOG(1) << "reject";
    }
    return cost_delta < 0;
  }
 private:
  struct Undo {
    Undo(int c, uint64 b) : card(c), bitset(b) {}
    int card;
    uint64 bitset;
  };

  int Cost(int intersection_size) {
    return abs(intersection_size - 1);
  }

  void Backtrack() {
    for (int i = 0; i < backtrack_.size(); ++i) {
      cards_[backtrack_[i].card] = backtrack_[i].bitset;
    }
  }

  void Save(int card) {
    backtrack_.push_back(Undo(card, cards_[card]));
  }

  bool CheckCards() {
    for (int i = 0; i < num_cards_; ++i) {
      if (num_symbols_per_card_ != BitCount64(cards_[i])) {
        VLOG(1) << "card " << i << " has bitset of size "
                  << BitCount64(cards_[i]);
        return false;
      }
    }
    return true;
  }

  const int num_cards_;
  const int num_symbols_;
  const int num_symbols_per_card_;
  vector<uint64> cards_;
  vector<vector<int> > costs_;
  vector<Undo> backtrack_;
};

void SolveDobble(int num_cards, int num_symbols, int num_symbols_per_card) {
  LOG(INFO) << "Solving dobble assignment problem:";
  LOG(INFO) << "  - " << num_cards << " cards";
  LOG(INFO) << "  - " << num_symbols << " symbols";
  LOG(INFO) << "  - " << num_symbols_per_card << " symbols per card";

  Solver solver("dobble");
  vector<vector<IntVar*> > vars(num_cards);
  vector<IntVar*> all_vars;
  for (int card_index = 0; card_index < num_cards; ++card_index) {
    solver.MakeBoolVarArray(num_symbols,
                            StringPrintf("card_%i_", card_index),
                            &vars[card_index]);
    for (int symbol_index = 0;
         symbol_index < num_symbols;
         ++symbol_index) {
      all_vars.push_back(vars[card_index][symbol_index]);
    }
  }
  vector<IntVar*> slack_vars;
  for (int card1 = 0; card1 < num_cards; ++card1) {
    for (int card2 = 0; card2 < num_cards; ++card2) {
      if (card1 != card2) {
        slack_vars.push_back(AddIntersectionVar(&solver,
                                                vars[card1],
                                                vars[card2],
                                                num_symbols_per_card));
      }
    }
  }

  for (int card = 0; card < num_cards; ++card) {
    solver.AddConstraint(solver.MakeSumEquality(vars[card],
                                                num_symbols_per_card));
  }

  for (int symbol_index = 0; symbol_index < num_symbols; ++symbol_index) {
    vector<IntVar*> tmp;
    for (int card_index = 0; card_index < num_cards; ++card_index) {
      tmp.push_back(vars[card_index][symbol_index]);
     }
    solver.AddConstraint(solver.MakeSumEquality(tmp, num_symbols_per_card));
  }

  LOG(INFO) << "Solving with LNS";
  LOG(INFO) << "  - lns_size = " << FLAGS_lns_size;
  LOG(INFO) << "  - lns_limit = " << FLAGS_lns_limit;
  LOG(INFO) << "  - fail_limit = " << FLAGS_fail_limit;

  DecisionBuilder* const build_db = solver.MakePhase(all_vars,
                                                     Solver::CHOOSE_RANDOM,
                                                     Solver::ASSIGN_MAX_VALUE);

  const int kNhoodLimit = 1000;
  LocalSearchOperator* const switch_operator =
      solver.RevAlloc(new SwitchSymbols(all_vars.data(),
                                        all_vars.size(),
                                        num_cards,
                                        num_symbols,
                                        num_symbols_per_card));
  LocalSearchOperator* const cycle_operator =
      solver.RevAlloc(new CycleSymbols(all_vars.data(),
                                       all_vars.size(),
                                       num_cards,
                                       num_symbols,
                                       num_symbols_per_card));
  LocalSearchOperator* const long_cycle_operator_limited =
      solver.MakeNeighborhoodLimit(
          solver.RevAlloc(new CycleNeighborhood(all_vars.data(),
                                                all_vars.size(),
                                                FLAGS_lns_size,
                                                num_cards,
                                                num_symbols,
                                                num_symbols_per_card)),
          kNhoodLimit);
  LocalSearchOperator* const long_cycle_operator_unlimited =
      solver.RevAlloc(new CycleNeighborhood(all_vars.data(),
                                            all_vars.size(),
                                            FLAGS_lns_size,
                                            num_cards,
                                            num_symbols,
                                            num_symbols_per_card));
  // LocalSearchOperator* const cross_lns_operator_limited =
  //     solver.MakeNeighborhoodLimit(
  //         solver.RevAlloc(new CrossLns(all_vars.data(),
  //                                      all_vars.size(),
  //                                      FLAGS_lns_size,
  //                                      num_cards,
  //                                      num_symbols,
  //                                      num_symbols_per_card)),
  //         kNhoodLimit);
  LocalSearchOperator* const card_lns_operator_limited =
      solver.MakeNeighborhoodLimit(
          solver.RevAlloc(new CardLns(all_vars.data(),
                                      all_vars.size(),
                                      FLAGS_lns_size,
                                      num_cards,
                                      num_symbols)),
          kNhoodLimit);
  LocalSearchOperator* const card_lns_operator_unlimited =
      solver.RevAlloc(new CardLns(all_vars.data(),
                                  all_vars.size(),
                                  FLAGS_lns_size,
                                  num_cards,
                                  num_symbols));

  vector<LocalSearchOperator*> operators;
  operators.push_back(switch_operator);
  operators.push_back(card_lns_operator_limited);
  operators.push_back(long_cycle_operator_limited);
  operators.push_back(cycle_operator);
  //  operators.push_back(card_lns_operator_unlimited);
  operators.push_back(long_cycle_operator_unlimited);
  LocalSearchOperator* const concat =
      solver.ConcatenateOperators(operators, true);
  SearchLimit* const lns_limit =
      solver.MakeLimit(kint64max, kint64max, FLAGS_lns_limit, kint64max);
  DecisionBuilder* const ls_db = solver.MakeSolveOnce(build_db, lns_limit);
  vector<LocalSearchFilter*> filters;
  filters.push_back(solver.RevAlloc(new DobbleFilter(all_vars.data(),
                                                     all_vars.size(),
                                                     num_cards,
                                                     num_symbols,
                                                     num_symbols_per_card)));
  LocalSearchPhaseParameters* const parameters =
      solver.MakeLocalSearchPhaseParameters(concat, ls_db, NULL, filters);
  DecisionBuilder* const final_db =
      solver.MakeLocalSearchPhase(all_vars, build_db, parameters);

  IntVar* const objective_var = solver.MakeSum(slack_vars)->Var();

  vector<SearchMonitor*> monitors;
  OptimizeVar* const optimize = solver.MakeMinimize(objective_var, 1);
  monitors.push_back(optimize);
  SearchMonitor* const log = solver.MakeSearchLog(100000, optimize);
  monitors.push_back(log);
  SearchLimit* const fail_limit =
      solver.MakeLimit(kint64max, kint64max, FLAGS_fail_limit, kint64max);
  monitors.push_back(fail_limit);
  solver.Solve(final_db, monitors);
}
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  const int kCards = 57;
  const int kSymbols = 57;
  const int kSymbolsPerCard = 8;
  operations_research::SolveDobble(kCards,
                                   kSymbols,
                                   kSymbolsPerCard);
  return 0;
}
