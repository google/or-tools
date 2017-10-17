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


// This problem is inspired by the Dobble game (aka Spot-It in the
// USA).  In this game, we have 57 cards, 57 symbols, and 8 symbols
// per card.  We want to assign symbols per card such that any two
// cards have exactly one symbol in common. These numbers can be
// generalized: we have N cards, each with K different symbols, and
// there are N different symbols overall.
//
// This is a feasability problem. We transform that into an
// optimization problem where we penalize cards whose intersection is
// of cardinality different from 1. A feasible solution of the
// original problem is a solution with a zero cost.
//
// Furthermore, we solve this problem using local search, and with a
// dedicated constraint.
//
// The purpose of the example is to demonstrates how to write local
// search operators and local search filters.

#include <algorithm>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/map_util.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/bitset.h"
#include "ortools/base/random.h"

DEFINE_int32(symbols_per_card, 8, "Number of symbols per card.");
DEFINE_int32(ls_seed, 1,
             "Seed for the random number generator (used by "
             "the Local Neighborhood Search).");
DEFINE_bool(use_filter, true, "Use filter in the local search to prune moves.");
DEFINE_int32(num_swaps, 4,
             "If num_swap > 0, the search for an optimal "
             "solution will be allowed to use an operator that swaps the "
             "symbols of up to num_swap pairs ((card1, symbol on card1), "
             "(card2, symbol on card2)).");
DEFINE_int32(time_limit_in_ms, 60000,
             "Time limit for the global search in ms.");

namespace operations_research {

// ----- Dedicated constraint to count the symbols shared by two cards -----

// This constraint maintains:
// sum_i(card1_symbol_vars[i]*card2_symbol_vars[i]) == count_var.
// with all card_symbol_vars[i] being boolean variables.
class SymbolsSharedByTwoCardsConstraint : public Constraint {
 public:
  // This constructor does not take any ownership on its arguments.
  SymbolsSharedByTwoCardsConstraint(
      Solver* const solver, const std::vector<IntVar*>& card1_symbol_vars,
      const std::vector<IntVar*>& card2_symbol_vars,
      IntVar* const num_symbols_in_common_var)
      : Constraint(solver),
        card1_symbol_vars_(card1_symbol_vars),
        card2_symbol_vars_(card2_symbol_vars),
        num_symbols_(card1_symbol_vars.size()),
        num_symbols_in_common_var_(num_symbols_in_common_var) {
    // Checks that cards have the same size.
    CHECK_EQ(card1_symbol_vars.size(), card2_symbol_vars.size());

    // Verify that we are really dealing with boolean variables.
    for (int i = 0; i < num_symbols_; ++i) {
      CHECK_GE(card1_symbol_vars_[i]->Min(), 0);
      CHECK_GE(card2_symbol_vars_[i]->Min(), 0);
      CHECK_LE(card1_symbol_vars_[i]->Max(), 1);
      CHECK_LE(card2_symbol_vars_[i]->Max(), 1);
    }
  }

  ~SymbolsSharedByTwoCardsConstraint() override {}

  // Adds observers (named Demon) to variable events. These demons are
  // responsible for implementing the propagation algorithm of the
  // constraint.
  void Post() override {
    // Create a demon 'global_demon' that will bind events on
    // variables to the calling of the 'InitialPropagate()' method. As
    // this method is expensive, 'global_demon' has a low priority. As
    // such, InitialPropagate will be called after all normal demons
    // and constraints have reached a fixed point. Note
    // that ownership of the 'global_demon' belongs to the solver.
    Demon* const global_demon =
        solver()->MakeDelayedConstraintInitialPropagateCallback(this);
    // Attach to all variables.
    for (int i = 0; i < num_symbols_; ++i) {
      card1_symbol_vars_[i]->WhenBound(global_demon);
      card2_symbol_vars_[i]->WhenBound(global_demon);
    }
    // Attach to cardinality variable.
    num_symbols_in_common_var_->WhenBound(global_demon);
  }

  // This is the main propagation method.
  //
  // It scans all card1_symbol_vars * card2_symbol_vars and increments 3
  // counters:
  //  - min_symbols_in_common if both booleans variables are bound to true.
  //  - max_symbols_in_common if both booleans are not bound to false.
  //
  // Then we know that num_symbols_in_common_var is in the range
  //    [min_symbols_in_common .. max_symbols_in_common].
  //
  // Now, if num_symbols_in_common_var->Max() ==
  // min_symbols_in_common, then all products that contribute to
  // max_symbols_in_common but not to min_symbols_in_common should be
  // set to 0.
  //
  // Conversely, if num_symbols_in_common_var->Min() ==
  // max_symbols_in_common, then all products that contribute to
  // max_symbols_in_common should be set to 1.
  void InitialPropagate() override {
    int max_symbols_in_common = 0;
    int min_symbols_in_common = 0;
    for (int i = 0; i < num_symbols_; ++i) {
      if (card1_symbol_vars_[i]->Min() == 1 &&
          card2_symbol_vars_[i]->Min() == 1) {
        min_symbols_in_common++;
      }
      if (card1_symbol_vars_[i]->Max() == 1 &&
          card2_symbol_vars_[i]->Max() == 1) {
        max_symbols_in_common++;
      }
    }
    num_symbols_in_common_var_->SetRange(min_symbols_in_common,
                                         max_symbols_in_common);
    // If min_symbols_in_common == max_symbols_in_common, it means
    // that num_symbols_in_common_var_ is already fully determined: we
    // have nothing to do.
    if (min_symbols_in_common == max_symbols_in_common) {
      DCHECK_EQ(min_symbols_in_common, num_symbols_in_common_var_->Max());
      DCHECK_EQ(min_symbols_in_common, num_symbols_in_common_var_->Min());
      return;
    }
    if (num_symbols_in_common_var_->Max() == min_symbols_in_common) {
      // All undecided product terms should be forced to 0.
      for (int i = 0; i < num_symbols_; ++i) {
        // If both Min() are 0, then we can't force either variable to
        // be zero (even if we know that their product is zero),
        // because either variable could be 1 as long as the other is
        // 0.
        if (card1_symbol_vars_[i]->Min() == 1 &&
            card2_symbol_vars_[i]->Min() == 0) {
          card2_symbol_vars_[i]->SetValue(0);
        } else if (card2_symbol_vars_[i]->Min() == 1 &&
                   card1_symbol_vars_[i]->Min() == 0) {
          card1_symbol_vars_[i]->SetValue(0);
        }
      }
    } else if (num_symbols_in_common_var_->Min() == max_symbols_in_common) {
      // All undecided product terms should be forced to 1.
      for (int i = 0; i < num_symbols_; ++i) {
        if (card1_symbol_vars_[i]->Max() == 1 &&
            card2_symbol_vars_[i]->Max() == 1) {
          // Note that we also force already-decided product terms,
          // but this doesn't change anything.
          card1_symbol_vars_[i]->SetValue(1);
          card2_symbol_vars_[i]->SetValue(1);
        }
      }
    }
  }

 private:
  std::vector<IntVar*> card1_symbol_vars_;
  std::vector<IntVar*> card2_symbol_vars_;
  const int num_symbols_;
  IntVar* const num_symbols_in_common_var_;
};

// Creates two integer variables: one that counts the number of
// symbols common to two cards, and one that counts the absolute
// difference between the first var and 1 (i.e. the violation of the
// objective). Returns the latter (both vars are owned by the Solver
// anyway).
IntVar* CreateViolationVar(Solver* const solver,
                           const std::vector<IntVar*>& card1_symbol_vars,
                           const std::vector<IntVar*>& card2_symbol_vars,
                           int num_symbols_per_card) {
  IntVar* const num_symbols_in_common_var =
      solver->MakeIntVar(0, num_symbols_per_card);
  // RevAlloc transfers the ownership of the constraint to the solver.
  solver->AddConstraint(solver->RevAlloc(new SymbolsSharedByTwoCardsConstraint(
      solver, card1_symbol_vars, card2_symbol_vars,
      num_symbols_in_common_var)));
  return solver->MakeAbs(solver->MakeSum(num_symbols_in_common_var, -1))->Var();
}

// ---------- Local Search ----------

// The "local search", or "local neighborhood search", works like
// this: starting from a given solution to the problem, other
// solutions in its neighborhood are generated from it, some of them
// might be selected (because they're better, for example) to become a
// starting point for other neighborhood searches, and so on.. The
// detailed search algorithm can vary and depends on the problem to
// solve.
//
// The fundamental building block for the local search is the "search
// operator", which has three fundamental methods in its API:
//
// - Its constructor, which keeps (mutable) references to the
// solver's internal variables (here, the card symbol variables).
//
// - OnStart(), which is called at the start of a local search, and
// after each solution (i.e. when the local search starts again from
// that new solution). The solver variables are supposed to represent
// a valid solution at this point. This method is used by the search
// operator to initialize its state and be ready to start the
// exploration of the neighborhood of the given solution.
//
// - MakeOneNeighbor(), which picks a neighbor of the initial
// solution, and changes the solver's internal variables accordingly
// to represent that new state.
//
// All local search operators on this problem will derive from the
// parent class below, which contains some shared code to store a
// compact representation of which symbols appeal on each cards.
class DobbleOperator : public IntVarLocalSearchOperator {
 public:
  DobbleOperator(const std::vector<IntVar*>& card_symbol_vars, int num_cards,
                 int num_symbols, int num_symbols_per_card)
      : IntVarLocalSearchOperator(card_symbol_vars),
        num_cards_(num_cards),
        num_symbols_(num_symbols),
        num_symbols_per_card_(num_symbols_per_card),
        symbols_per_card_(num_cards) {
    CHECK_GT(num_cards, 0);
    CHECK_GT(num_symbols, 0);
    CHECK_GT(num_symbols_per_card, 0);
    for (int card = 0; card < num_cards; ++card) {
      symbols_per_card_[card].assign(num_symbols_per_card, -1);
    }
  }

  ~DobbleOperator() override {}

 protected:
  // OnStart() simply stores the current symbols per card in
  // symbols_per_card_, and defers further initialization to the
  // subclass's InitNeighborhoodSearch() method.
  void OnStart() override {
    for (int card = 0; card < num_cards_; ++card) {
      int found = 0;
      for (int symbol = 0; symbol < num_symbols_; ++symbol) {
        if (Value(VarIndex(card, symbol)) == 1) {
          symbols_per_card_[card][found++] = symbol;
        }
      }
      DCHECK_EQ(num_symbols_per_card_, found);
    }
    InitNeighborhoodSearch();
  }

  virtual void InitNeighborhoodSearch() = 0;

  // Find the index of the variable corresponding to the given symbol
  // on the given card.
  int VarIndex(int card, int symbol) { return card * num_symbols_ + symbol; }

  // Move symbol1 from card1 to card2, and symbol2 from card2 to card1.
  void SwapTwoSymbolsOnCards(int card1, int symbol1, int card2, int symbol2) {
    SetValue(VarIndex(card1, symbol1), 0);
    SetValue(VarIndex(card2, symbol2), 0);
    SetValue(VarIndex(card1, symbol2), 1);
    SetValue(VarIndex(card2, symbol1), 1);
  }

  const int num_cards_;
  const int num_symbols_;
  const int num_symbols_per_card_;
  std::vector<std::vector<int> > symbols_per_card_;
};

// ----- Swap 2 symbols -----

// This operator explores *all* pairs (card1, some symbol on card1),
// (card2, some symbol on card2) and swaps the symbols between the two
// cards.
//
// Note that this could create invalid moves (for example, by adding a
// symbol to a card that already had it); see the DobbleFilter class
// below to see how we filter those out.
class SwapSymbols : public DobbleOperator {
 public:
  SwapSymbols(const std::vector<IntVar*>& card_symbol_vars, int num_cards,
              int num_symbols, int num_symbols_per_card)
      : DobbleOperator(card_symbol_vars, num_cards, num_symbols,
                       num_symbols_per_card),
        current_card1_(-1),
        current_card2_(-1),
        current_symbol1_(-1),
        current_symbol2_(-1) {}

  ~SwapSymbols() override {}

  // Finds the next swap, returns false when it has finished.
  bool MakeOneNeighbor() override {
    if (!PickNextSwap()) {
      VLOG(1) << "finished neighborhood";
      return false;
    }

    const int symbol1 = symbols_per_card_[current_card1_][current_symbol1_];
    const int symbol2 = symbols_per_card_[current_card2_][current_symbol2_];
    SwapTwoSymbolsOnCards(current_card1_, symbol1, current_card2_, symbol2);
    return true;
  }

 private:
  // Reinit the exploration loop.
  void InitNeighborhoodSearch() override {
    current_card1_ = 0;
    current_card2_ = 1;
    current_symbol1_ = 0;
    current_symbol2_ = -1;
  }

  // Compute the next move. It returns false when there are none.
  bool PickNextSwap() {
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

  int current_card1_;
  int current_card2_;
  int current_symbol1_;
  int current_symbol2_;
};

// Multiple swaps of two symbols. This operator is an expanded version
// of the previous operator.
//
// At each step, it will pick a number num_swaps at random in
// [2 .. max_num_swaps], and then pick num_swaps random pairs (card1,
// some symbol on card1), (card2, some symbol on card2), and swap the
// symbols of each pair.
//
// As the search space (the "neighborhood") is huge, we use a
// randomized "infinite" version instead of an iterative, exhaustive
// one.
class SwapSymbolsOnCardPairs : public DobbleOperator {
 public:
  SwapSymbolsOnCardPairs(const std::vector<IntVar*>& card_symbol_vars,
                         int num_cards, int num_symbols,
                         int num_symbols_per_card, int max_num_swaps)
      : DobbleOperator(card_symbol_vars, num_cards, num_symbols,
                       num_symbols_per_card),
        rand_(FLAGS_ls_seed),
        max_num_swaps_(max_num_swaps) {
    CHECK_GE(max_num_swaps, 2);
  }

  ~SwapSymbolsOnCardPairs() override {}

 protected:
  bool MakeOneNeighbor() override {
    const int num_swaps = rand_.Uniform(max_num_swaps_ - 1) + 2;
    for (int i = 0; i < num_swaps; ++i) {
      const int card_1 = rand_.Uniform(num_cards_);
      const int symbol_index_1 = rand_.Uniform(num_symbols_per_card_);
      const int symbol_1 = symbols_per_card_[card_1][symbol_index_1];
      const int card_2 = rand_.Uniform(num_cards_);
      const int symbol_index_2 = rand_.Uniform(num_symbols_per_card_);
      const int symbol_2 = symbols_per_card_[card_2][symbol_index_2];
      SwapTwoSymbolsOnCards(card_1, symbol_1, card_2, symbol_2);
    }
    return true;
  }

  void InitNeighborhoodSearch() override {}

 private:
  ACMRandom rand_;
  const int max_num_swaps_;
};

// ----- Local Search Filter -----

// A filter is responsible for rejecting a local search move faster
// than what the propagation of the constraint solver would do.
// Its API consists in:
//   - The constructor, which takes as input a reference to all the
//     variables relevant to the filter.
//   - OnSynchronize(), called at the beginning of the search and
//     after each move to a new solution (when the local search
//     restarts from it).
//   - Accept(), which takes as input an attempted move (in the form
//     of a Delta to tentatively apply to the variables), and returns
//     true iff this move is found valid.
//
// To decide if a move is valid, first this DobbleFilter builds a
// bitmap of symbols per card.  Then for each move, it updates the
// bitmap according to the move and checks the following constraints:
// - First, each card still has num_symbols_per_card symbols.  - The
// cost of the assignment described by the move is better than the
// current one.

// After the check is done, the original bitmap is restored if the
// move was rejected, so as to be ready for the next evaluation.
//
// Please note that this filter uses a fixed size bitset and
// effectively limits the number of cards to 63, and thus the number
// of symbols per card to 8.
class DobbleFilter : public IntVarLocalSearchFilter {
 public:
  DobbleFilter(const std::vector<IntVar*>& card_symbol_vars, int num_cards,
               int num_symbols, int num_symbols_per_card)
      : IntVarLocalSearchFilter(card_symbol_vars),
        num_cards_(num_cards),
        num_symbols_(num_symbols),
        num_symbols_per_card_(num_symbols_per_card),
        temporary_bitset_(0),
        symbol_bitmask_per_card_(num_cards, 0),
        violation_costs_(num_cards_, std::vector<int>(num_cards_, 0)) {}

  // We build the current bitmap and the matrix of violation cost
  // between any two cards.
  void OnSynchronize(const Assignment* delta) override {
    symbol_bitmask_per_card_.assign(num_cards_, 0);
    for (int card = 0; card < num_cards_; ++card) {
      for (int symbol = 0; symbol < num_symbols_; ++symbol) {
        if (Value(VarIndex(card, symbol))) {
          SetBit64(&symbol_bitmask_per_card_[card], symbol);
        }
      }
    }
    for (int card1 = 0; card1 < num_cards_; ++card1) {
      for (int card2 = 0; card2 < num_cards_; ++card2) {
        violation_costs_[card1][card2] = ViolationCost(BitCount64(
            symbol_bitmask_per_card_[card1] & symbol_bitmask_per_card_[card2]));
      }
    }
    DCHECK(CheckCards());
  }

  // The LocalSearchFilter::Accept() API also takes a deltadelta,
  // which is the difference between the current delta and the last
  // delta that was given to Accept() -- but we don't use it here.
  bool Accept(const Assignment* delta,
              const Assignment* unused_deltadelta) override {
    const Assignment::IntContainer& solution_delta = delta->IntVarContainer();
    const int solution_delta_size = solution_delta.Size();

    // The input const Assignment* delta given to Accept() may
    // actually contain "Deactivated" elements, which represent
    // variables that have been freed -- they are not bound to a
    // single value anymore. This happens with LNS-type (Large
    // Neighborhood Search) LocalSearchOperator, which are not used in
    // this example as of 2012-01; and we refer the reader to
    // ./routing.cc for an example of such LNS-type operators.
    //
    // For didactical purposes, we will assume for a moment that a
    // LNS-type operator might be applied. The Filter will still be
    // called, but our DobbleFilter here won't be able to work, since
    // it needs every variable to be bound (i.e. have a fixed value),
    // in the assignment that it considers. Therefore, we include here
    // a snippet of code that will detect if the input assignment is
    // not fully bound. For further details, read ./routing.cc -- but
    // we strongly advise the reader to first try and understand all
    // of this file.
    for (int i = 0; i < solution_delta_size; ++i) {
      if (!solution_delta.Element(i).Activated()) {
        VLOG(1) << "Element #" << i << " of the delta assignment given to"
                << " DobbleFilter::Accept() is not activated (i.e. its variable"
                << " is not bound to a single value anymore). This means that"
                << " we are in a LNS phase, and the DobbleFilter won't be able"
                << " to filter anything. Returning true.";
        return true;
      }
    }
    VLOG(1) << "No LNS, size = " << solution_delta_size;

    // Collect the set of cards that have been modified by this move.
    std::vector<int> touched_cards;
    ComputeTouchedCards(solution_delta, &touched_cards);

    // Check basic metrics to fail fast.
    if (!CheckCards()) {
      RestoreBitsetPerCard();
      DCHECK(CheckCards());
      VLOG(1) << "reject by size";
      return false;
    }

    // Compute new cost.
    const int cost_delta = ComputeNewCost(touched_cards);

    // Reset the data structure to the state before the evaluation.
    RestoreBitsetPerCard();

    // And exit (this is only valid for a greedy descent and would
    // reject valid moves in tabu search for instance).
    if (cost_delta >= 0) {
      VLOG(1) << "reject";
    }
    return cost_delta < 0;
  }

 private:
  // Undo information after an evaluation.
  struct UndoChange {
    UndoChange(int c, uint64 b) : card(c), bitset(b) {}
    int card;
    uint64 bitset;
  };

  int VarIndex(int card, int symbol) { return card * num_symbols_ + symbol; }

  void ClearBitset() { temporary_bitset_ = 0; }

  // For each touched card, compare against all others to compute the
  // delta in term of cost. We use an bitset to avoid counting twice
  // between two cards appearing in the local search move.
  int ComputeNewCost(const std::vector<int>& touched_cards) {
    ClearBitset();
    int cost_delta = 0;
    for (int i = 0; i < touched_cards.size(); ++i) {
      const int touched = touched_cards[i];
      SetBit64(&temporary_bitset_, touched);
      const uint64 card_bitset = symbol_bitmask_per_card_[touched];
      const std::vector<int>& row_cost = violation_costs_[touched];
      for (int other_card = 0; other_card < num_cards_; ++other_card) {
        if (!IsBitSet64(&temporary_bitset_, other_card)) {
          cost_delta += ViolationCost(
              BitCount64(card_bitset & symbol_bitmask_per_card_[other_card]));
          cost_delta -= row_cost[other_card];
        }
      }
    }
    return cost_delta;
  }

  // Collects all card indices appearing in the local search move.
  void ComputeTouchedCards(const Assignment::IntContainer& solution_delta,
                           std::vector<int>* const touched_cards) {
    ClearBitset();
    const int solution_delta_size = solution_delta.Size();
    const int kUnassigned = -1;
    for (int index = 0; index < solution_delta_size; ++index) {
      int64 touched_var = kUnassigned;
      FindIndex(solution_delta.Element(index).Var(), &touched_var);
      CHECK_NE(touched_var, kUnassigned);
      const int card = touched_var / num_symbols_;
      const int symbol = touched_var % num_symbols_;
      const int new_value = solution_delta.Element(index).Value();
      if (!IsBitSet64(&temporary_bitset_, card)) {
        SaveRestoreInformation(card);
        touched_cards->push_back(card);
        SetBit64(&temporary_bitset_, card);
      }
      if (new_value) {
        SetBit64(&symbol_bitmask_per_card_[card], symbol);
      } else {
        ClearBit64(&symbol_bitmask_per_card_[card], symbol);
      }
    }
  }

  // Undo all modifications done when evaluating a move.
  void RestoreBitsetPerCard() {
    for (int i = 0; i < restore_information_.size(); ++i) {
      symbol_bitmask_per_card_[restore_information_[i].card] =
          restore_information_[i].bitset;
    }
    restore_information_.clear();
  }

  // Stores undo information for a given card.
  void SaveRestoreInformation(int card) {
    restore_information_.push_back(
        UndoChange(card, symbol_bitmask_per_card_[card]));
  }

  // Checks that after the local search move, each card would still have
  // num_symbols_per_card symbols on it.
  bool CheckCards() {
    for (int i = 0; i < num_cards_; ++i) {
      if (num_symbols_per_card_ != BitCount64(symbol_bitmask_per_card_[i])) {
        VLOG(1) << "card " << i << " has bitset of size "
                << BitCount64(symbol_bitmask_per_card_[i]);
        return false;
      }
    }
    return true;
  }

  int ViolationCost(uint64 cardinality) const {
    return (cardinality > 0 ? cardinality - 1 : 1);
  }

  const int num_cards_;
  const int num_symbols_;
  const int num_symbols_per_card_;
  uint64 temporary_bitset_;
  std::vector<uint64> symbol_bitmask_per_card_;
  std::vector<std::vector<int> > violation_costs_;
  std::vector<UndoChange> restore_information_;
};

// ----- Main Method -----

void SolveDobble(int num_cards, int num_symbols, int num_symbols_per_card) {
  LOG(INFO) << "Solving dobble assignment problem:";
  LOG(INFO) << "  - " << num_cards << " cards";
  LOG(INFO) << "  - " << num_symbols << " symbols";
  LOG(INFO) << "  - " << num_symbols_per_card << " symbols per card";

  // Creates the solver.
  Solver solver("dobble");
  // Creates the matrix of boolean variables (cards * symbols).
  std::vector<std::vector<IntVar*> > card_symbol_vars(num_cards);
  std::vector<IntVar*> all_card_symbol_vars;
  for (int card_index = 0; card_index < num_cards; ++card_index) {
    solver.MakeBoolVarArray(num_symbols, StringPrintf("card_%i_", card_index),
                            &card_symbol_vars[card_index]);
    for (int symbol_index = 0; symbol_index < num_symbols; ++symbol_index) {
      all_card_symbol_vars.push_back(
          card_symbol_vars[card_index][symbol_index]);
    }
  }
  // Creates cardinality intersection variables and remember the
  // violation variables.
  std::vector<IntVar*> violation_vars;
  for (int card1 = 0; card1 < num_cards; ++card1) {
    for (int card2 = 0; card2 < num_cards; ++card2) {
      if (card1 != card2) {
        violation_vars.push_back(
            CreateViolationVar(&solver, card_symbol_vars[card1],
                               card_symbol_vars[card2], num_symbols_per_card));
      }
    }
  }
  // Create the objective variable.
  IntVar* const objective_var = solver.MakeSum(violation_vars)->Var();

  // Add constraint: there must be exactly num_symbols_per_card
  // symbols per card.
  for (int card = 0; card < num_cards; ++card) {
    solver.AddConstraint(
        solver.MakeSumEquality(card_symbol_vars[card], num_symbols_per_card));
  }

  // IMPORTANT OPTIMIZATION:
  // Add constraint: each symbol appears on exactly
  // num_symbols_per_card cards (i.e. symbols are evenly
  // distributed). This constraint is actually redundant, because it
  // is a (non-trivial) consequence of the other constraints and of
  // the model. But adding it makes the search go faster.
  for (int symbol_index = 0; symbol_index < num_symbols; ++symbol_index) {
    std::vector<IntVar*> tmp;
    for (int card_index = 0; card_index < num_cards; ++card_index) {
      tmp.push_back(card_symbol_vars[card_index][symbol_index]);
    }
    solver.AddConstraint(solver.MakeSumEquality(tmp, num_symbols_per_card));
  }

  // Search.
  LOG(INFO) << "Solving with Local Search";
  LOG(INFO) << "  - time limit = " << FLAGS_time_limit_in_ms << " ms";

  // Start a DecisionBuilder phase to find a first solution, using the
  // strategy "Pick some random, yet unassigned card symbol variable
  // and set its value to 1".
  DecisionBuilder* const build_db = solver.MakePhase(
      all_card_symbol_vars, Solver::CHOOSE_RANDOM,  // Solver::IntVarStrategy
      Solver::ASSIGN_MAX_VALUE);                    // Solver::IntValueStrategy

  // Creates local search operators.
  std::vector<LocalSearchOperator*> operators;
  LocalSearchOperator* const switch_operator = solver.RevAlloc(new SwapSymbols(
      all_card_symbol_vars, num_cards, num_symbols, num_symbols_per_card));
  operators.push_back(switch_operator);
  LOG(INFO) << "  - add switch operator";
  if (FLAGS_num_swaps > 0) {
    LocalSearchOperator* const swaps_operator = solver.RevAlloc(
        new SwapSymbolsOnCardPairs(all_card_symbol_vars, num_cards, num_symbols,
                                   num_symbols_per_card, FLAGS_num_swaps));
    operators.push_back(swaps_operator);
    LOG(INFO) << "  - add swaps operator with at most " << FLAGS_num_swaps
              << " swaps";
  }

  // Creates filter.
  std::vector<LocalSearchFilter*> filters;
  if (FLAGS_use_filter) {
    filters.push_back(solver.RevAlloc(new DobbleFilter(
        all_card_symbol_vars, num_cards, num_symbols, num_symbols_per_card)));
  }

  // Main decision builder that regroups the first solution decision
  // builder and the combination of local search operators and
  // filters.
  DecisionBuilder* const final_db = solver.MakeLocalSearchPhase(
      all_card_symbol_vars, build_db,
      solver.MakeLocalSearchPhaseParameters(
          solver.ConcatenateOperators(operators, true),
          nullptr,  // Sub decision builder, not needed here.
          nullptr,  // Limit the search for improving move, we will stop
                    // the exploration of the local search at the first
                    // improving solution (first accept).
          filters));

  std::vector<SearchMonitor*> monitors;
  // Optimize var search monitor.
  OptimizeVar* const optimize = solver.MakeMinimize(objective_var, 1);
  monitors.push_back(optimize);

  // Search log.
  SearchMonitor* const log = solver.MakeSearchLog(100000, optimize);
  monitors.push_back(log);

  // Search limit.
  SearchLimit* const time_limit =
      solver.MakeLimit(FLAGS_time_limit_in_ms, kint64max, kint64max, kint64max);
  monitors.push_back(time_limit);

  // And solve!
  solver.Solve(final_db, monitors);
}
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  // These constants comes directly from the dobble game.
  // There are actually 55 cards, but we can create up to 57 cards.
  const int kSymbolsPerCard = FLAGS_symbols_per_card;
  const int kCards = kSymbolsPerCard * (kSymbolsPerCard - 1) + 1;
  const int kSymbols = kCards;
  operations_research::SolveDobble(kCards, kSymbols, kSymbolsPerCard);
  return 0;
}
