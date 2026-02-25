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

#include "ortools/sat/gate_congruence_closure.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/types/span.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph_base/connected_components.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/gate_utils.h"
#include "ortools/sat/lrat_proof_handler.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

GateCongruenceClosure::~GateCongruenceClosure() {
  if (!VLOG_IS_ON(1)) return;
  shared_stats_->AddStats({
      {"GateCongruenceClosure/dtime(int)", static_cast<int64_t>(total_dtime_)},
      {"GateCongruenceClosure/walltime(int)",
       static_cast<int64_t>(total_wtime_)},
      {"GateCongruenceClosure/gates", total_gates_},
      {"GateCongruenceClosure/units", total_num_units_},
      {"GateCongruenceClosure/equivalences", total_equivalences_},
  });
}

template <int arity>
void GateCongruenceClosure::AddToTruthTable(
    SatClause* clause,
    absl::flat_hash_map<std::array<BooleanVariable, arity>, TruthTableId>&
        ids) {
  CHECK_EQ(clause->size(), arity);
  std::array<BooleanVariable, arity> key;
  SmallBitset bitmask;
  FillKeyAndBitmask(clause->AsSpan(), absl::MakeSpan(key), bitmask);

  TruthTableId next_id(truth_tables_bitset_.size());
  auto [it, inserted] = ids.insert({key, next_id});
  const TruthTableId id = it->second;
  if (inserted) {
    truth_tables_inputs_.Add(key);
    truth_tables_bitset_.push_back(bitmask);
    if (lrat_proof_handler_ != nullptr) {
      tmp_ids_.push_back(id);
      tmp_clauses_.push_back(clause);
    }
  } else {
    const SmallBitset old = truth_tables_bitset_[id];

    // Remove one value.
    truth_tables_bitset_[id] &= bitmask;
    if (lrat_proof_handler_ != nullptr && old != truth_tables_bitset_[id]) {
      tmp_ids_.push_back(id);
      tmp_clauses_.push_back(clause);
    }
  }
}

namespace {

// Given a set of feasible assignment of two variables, recover the
// corresponding binary clauses.
void AppendBinaryClausesFromTruthTable(
    absl::Span<const BooleanVariable> vars, SmallBitset table,
    std::vector<std::pair<Literal, Literal>>* binary_used) {
  DCHECK_EQ(vars.size(), 2);
  for (int b = 0; b < 4; ++b) {
    if (((table >> b) & 1) == 0) {
      binary_used->emplace_back(Literal(vars[0], (b & 1) == 0),
                                Literal(vars[1], (b & 2) == 0));
    }
  }
}

}  // namespace

void GateCongruenceClosure::EarlyGateDetection() {
  PresolveTimer timer("EarlyGateDetection", logger_, time_limit_);
  timer.OverrideLogging(VLOG_IS_ON(2));

  // Allow to fill old_truth_tables_bitset_ with tight value.
  // Note that this change dtime and the solver behavior.
  StructureExtraction(timer);
  tmp_binary_clauses_.clear();
  if (lrat_proof_handler_ != nullptr) {
    lrat_proof_handler_->DeleteTemporaryBinaryClauses();
  }
}

// For now, we only keep size3 truth tables.
//
// Note that we include fixed variables. That is actually the point, since when
// only the target of a function is fixed, we can still infer reduction by using
// that function.
void GateCongruenceClosure::ProcessPreviousTruthTables(PresolveTimer& timer) {
  ids2_.clear();
  ids3_.clear();
  ids4_.clear();
  ids5_.clear();

  CompactVectorVector<TruthTableId, BooleanVariable> new_inputs;
  std::vector<Literal> tmp_literals;
  old_truth_tables_bitset_.clear();
  const int old_num_tables = truth_tables_inputs_.size();
  for (TruthTableId i(0); i < old_num_tables; ++i) {
    // For now, we only keep size 3.
    if (truth_tables_inputs_[i].size() != 3) continue;

    tmp_literals.clear();
    for (const BooleanVariable var : truth_tables_inputs_[i]) {
      tmp_literals.push_back(
          implication_graph_->RepresentativeOf(Literal(var, true)));
    }
    SmallBitset bitmask = truth_tables_bitset_[i];

    // TODO(user): FullyCanonicalizeTruthTable() loose some info, it isn't
    // completely clear why, and if we integrate ids2_ info as we canonicalize
    // gate, that might not be necessary.
    MakeAllInputsPositive(absl::MakeSpan(tmp_literals), bitmask);
    CanonicalizeTruthTable<Literal>(absl::MakeSpan(tmp_literals), bitmask);

    std::array<BooleanVariable, 3> new_key;
    for (int i = 0; i < 3; ++i) {
      CHECK(tmp_literals[i].IsPositive());
      new_key[i] = tmp_literals[i].Variable();
    }
    const TruthTableId new_id(new_inputs.size());
    auto [it, inserted] = ids3_.insert({new_key, new_id});
    if (inserted) {
      new_inputs.Add(new_key);
      old_truth_tables_bitset_.push_back(bitmask);
    } else {
      old_truth_tables_bitset_[it->second] &= bitmask;
    }
  }

  timer.AddCounter("old_t3", new_inputs.size());
  std::swap(truth_tables_inputs_, new_inputs);

  // TODO(user): We don't keep the actual bitmask, as it might be tricky
  // for the Lrat proof to keep the clauses needed for it. We expect to be
  // able to recover the same information using the current clause database.
  const SmallBitset all_one = GetNumBitsAtOne(8);
  truth_tables_bitset_.clear();
  truth_tables_bitset_.resize(truth_tables_inputs_.size(), all_one);
}

// Note that this is the "hot" part of the algo, once we have the and gates,
// the congruence closure should be quite fast.
void GateCongruenceClosure::ExtractAndGatesAndFillShortTruthTables(
    PresolveTimer& timer) {
  ProcessPreviousTruthTables(timer);
  truth_tables_clauses_.clear();
  tmp_ids_.clear();
  tmp_clauses_.clear();

  // We deal with binary clause a bit differently.
  //
  // Tricky: We still include binary clause between fixed literal that haven't
  // been cleaned up yet, as these are needed to really recover all gates.
  //
  // TODO(user): Ideally the detection code should be robust to that.
  // TODO(user): Maybe we should always have an hash-map of binary up to date?
  std::vector<std::pair<Literal, Literal>> binary_used;
  for (LiteralIndex a(0); a < implication_graph_->literal_size(); ++a) {
    // TODO(user): If we know we have too many implications for the time limit
    // We should just be better of not doing that loop at all.
    if (timer.WorkLimitIsReached()) break;
    if (implication_graph_->IsRedundant(Literal(a))) continue;
    const absl::Span<const Literal> implied =
        implication_graph_->Implications(Literal(a));
    timer.TrackHashLookups(implied.size());
    for (const Literal b : implied) {
      if (implication_graph_->IsRedundant(b)) continue;

      std::array<BooleanVariable, 2> key2;
      SmallBitset bitmask;
      FillKeyAndBitmask({Literal(a).Negated(), b}, absl::MakeSpan(key2),
                        bitmask);
      auto [it, inserted] = ids2_.insert({key2, bitmask});
      if (!inserted) {
        const SmallBitset old = it->second;
        it->second &= bitmask;
        if (it->second != old) {
          // This is either fixing or equivalence!
          //
          // Doing a run of DetectEquivalences() should fix that but then
          // new clause of size 3 might become binary, and the fix point might
          // require a lot of step. So it is important to do it here.
          const SmallBitset bitset2 = it->second;
          if (lrat_proof_handler_ != nullptr) {
            binary_used.clear();
            AppendBinaryClausesFromTruthTable(key2, bitset2, &binary_used);
          }
          // If we are equivalent, we always have 2 functions.
          // But if we fix a variable (like bitset2 = 0011) we just have one.
          const int num_added =
              ProcessTruthTable(key2, bitset2, {}, binary_used);
          CHECK_GE(num_added, 1) << std::bitset<4>(bitset2);
        }
      }
    }
  }
  timer.AddCounter("t2", ids2_.size());

  std::vector<Literal> candidates;
  for (SatClause* clause : clause_manager_->AllClausesInCreationOrder()) {
    if (timer.WorkLimitIsReached()) break;
    if (clause->size() == 0) continue;

    if (clause->size() == 3) {
      AddToTruthTable<3>(clause, ids3_);

      // The AND gate of size 3 should be detected by the short table code, no
      // need to do the algo here which should be slower.
      continue;
    } else if (clause->size() == 4) {
      AddToTruthTable<4>(clause, ids4_);
    } else if (clause->size() == 5) {
      AddToTruthTable<5>(clause, ids5_);
    }

    // Used for an optimization below.
    int min_num_implications = std::numeric_limits<int>::max();
    Literal lit_with_less_implications;

    const int clause_size = clause->size();
    timer.TrackSimpleLoop(clause_size);
    candidates.clear();
    for (const Literal l : clause->AsSpan()) {
      // TODO(user): using Implications() only considers pure binary
      // clauses and not at_most_one. Also, if we do transitive reduction, we
      // might skip important literals here. Maybe a better alternative is
      // to detect clauses that "propagate" l back when we probe l...
      const int num_implications = implication_graph_->Implications(l).size();
      if (num_implications < min_num_implications) {
        min_num_implications = num_implications;
        lit_with_less_implications = l;
      }

      if (num_implications >= clause_size - 1) {
        candidates.push_back(l);
      }
    }
    if (candidates.empty()) continue;
    if (min_num_implications == 0) continue;

    marked_.ResetAllToFalse();
    for (const Literal l : clause->AsSpan()) marked_.Set(l);
    const auto is_clause_literal = marked_.const_view();

    // There should be no duplicate in a clause.
    // And also not lit and lit.Negated(), but we don't check that here.
    CHECK_EQ(marked_.PositionsSetAtLeastOnce().size(), clause_size);

    // These bitsets will contain the intersection of all the negated literals
    // implied by one of the clause literal. It is used for an "optimization" as
    // a literal can only be a "target" of a bool_and if it implies all other
    // literals of the clause to false. So by contraposition, any literal should
    // directly imply such a target to false.
    //
    // This relies on the fact that for any a => b directly stored in
    // BinaryImplicationGraph, we should always have not(b) => not(a). This
    // only applies to the result of Implications() though, not the one we
    // can infer by transitivity.
    //
    // We start with the variables implied by "lit_with_less_implications" and
    // at each iteration, we take the intersection with the variables implied by
    // our current "target".
    //
    // TODO(user): SparseBitset<> does not support swap.
    auto* is_potential_target = &seen_;
    auto* next_is_potential_target = &next_seen_;

    // If we don't have lit_with_less_implications => not(target) then we
    // shouldn't have target => not(lit_with_less_implications).
    {
      is_potential_target->ResetAllToFalse();
      is_potential_target->Set(lit_with_less_implications);
      const absl::Span<const Literal> implications =
          implication_graph_->Implications(lit_with_less_implications);
      timer.TrackFastLoop(implications.size());
      for (const Literal implied : implications) {
        is_potential_target->Set(implied.Negated());
      }
    }

    for (const Literal target : candidates) {
      if (!(*is_potential_target)[target]) continue;

      int count = 0;
      next_is_potential_target->ResetAllToFalse();
      const absl::Span<const Literal> implications =
          implication_graph_->Implications(target);
      timer.TrackFastLoop(implications.size());
      for (const Literal implied : implications) {
        CHECK_NE(implied.Variable(), target.Variable());

        if (is_clause_literal[implied.Negated()]) {
          // Set next_is_potential_target to the intersection of
          // is_potential_target and the one we see here.
          if ((*is_potential_target)[implied.Negated()]) {
            next_is_potential_target->Set(implied.Negated());
          }
          ++count;
        }
      }
      std::swap(is_potential_target, next_is_potential_target);

      // Target should imply all other literal in the base clause to false.
      if (count < clause_size - 1) continue;

      // Using only the "count" require that there is no duplicates. But
      // depending when this is run in the inprocessing loop, we might have
      // some. Redo a pass to double check.
      int second_count = 0;
      for (const Literal implied : implications) {
        if (implied.Variable() == target.Variable()) continue;
        if (is_clause_literal[implied.Negated()]) {
          ++second_count;
          marked_.Clear(implied.Negated());
        }
      }

      // Restore is_clause_literal.
      for (const Literal l : clause->AsSpan()) {
        marked_.Set(l);
      }
      if (second_count != clause_size - 1) continue;

      // We have an and_gate !
      // Add the detected gate (its inputs are the negation of each clause
      // literal other than the target).
      gates_target_.push_back(target);
      gates_type_.push_back(kAndGateType);

      const GateId gate_id = GateId(gates_inputs_.Add({}));
      for (const Literal l : clause->AsSpan()) {
        if (l == target) continue;
        gates_inputs_.AppendToLastVector(l.Negated());
      }
      if (lrat_proof_handler_ != nullptr) {
        gates_clauses_.Add({clause});

        // Create temporary size 2 clauses for the needed binary.
        for (const Literal l : clause->AsSpan()) {
          if (l == target) continue;
          tmp_binary_clauses_.emplace_back(
              SatClause::Create({target.Negated(), l.Negated()}));
          gates_clauses_.AppendToLastVector(tmp_binary_clauses_.back().get());
        }
      }

      // Canonicalize.
      absl::Span<Literal> gate = gates_inputs_[gate_id];
      std::sort(gate.begin(), gate.end());

      // Even if we detected an and_gate from a base clause, we keep going
      // as their could me more than one. In the extreme of an "exactly_one",
      // a single base clause of size n will correspond to n and_gates !
    }
  }

  timer.AddCounter("and_gates", gates_inputs_.size());
}

int GateCongruenceClosure::CanonicalizeShortGate(GateId id) {
  // Deals with fixed input variable.
  absl::Span<Literal> inputs = gates_inputs_[id];
  int new_size = inputs.size();

  for (int i = 0; i < new_size;) {
    if (assignment_.LiteralIsAssigned(Literal(inputs[i]))) {
      new_size =
          RemoveFixedInput(i, assignment_.LiteralIsTrue(Literal(inputs[i])),
                           inputs.subspan(0, new_size), gates_type_[id]);
    } else {
      ++i;
    }
  }

  // Now canonicalize.
  //
  // Tricky: We always assume size 1 to be an "equivalence", so we do
  // need to lower the arity if the type is not an equivalence. Maybe a better
  // alternative would be to not always declare equivalence if we have x = f(y)
  // as x could be fixed...
  new_size = CanonicalizeFunctionTruthTable(
      gates_target_[id], inputs.subspan(0, new_size), gates_type_[id]);

  // Resize and return.
  if (new_size < gates_inputs_[id].size()) {
    gates_inputs_.Shrink(id, new_size);
  }
  CHECK_EQ(gates_type_[id] >> (1 << (gates_inputs_[id].size())), 0);
  return new_size;
}

int GateCongruenceClosure::ProcessTruthTable(
    absl::Span<const BooleanVariable> inputs, SmallBitset truth_table,
    absl::Span<const TruthTableId> ids_for_proof,
    absl::Span<const std::pair<Literal, Literal>> binary_used) {
  int num_detected = 0;
  for (int i = 0; i < inputs.size(); ++i) {
    if (!IsFunction(i, inputs.size(), truth_table)) continue;
    const int num_bits = inputs.size() - 1;
    ++num_detected;

    // Generate the function truth table as a type.
    unsigned int type = 0;
    unsigned int not_important = 0;
    for (int p = 0; p < (1 << num_bits); ++p) {
      // Expand from (num_bits == inputs.size() - 1) bits to (inputs.size())
      // bits.
      const int bigger_p = AddHoleAtPosition(i, p);

      if ((truth_table >> (bigger_p + (1 << i))) & 1) {
        // target is 1 at this position.
        type |= 1 << p;
        DCHECK_NE((truth_table >> bigger_p) & 1, 1);  // Proper function.
      } else {
        // Note that if there is no feasible assignment for a given p if
        // [(truth_table >> bigger_p) & 1] is not 1.
        //
        // Here we could learn smaller clause, but also we don't really care
        // what is the value of the target at that point p, so we use zero.
        if (((truth_table >> bigger_p) & 1) == 0) {
          not_important |= 1 << p;
        }
      }
    }

    // If not_important != 0, we might create many version of the "same"
    // function.
    SmallBitset mask = 0;
    while (true) {
      const GateId new_id(gates_target_.size());
      gates_target_.push_back(Literal(inputs[i], true));
      gates_inputs_.Add({});
      for (int j = 0; j < inputs.size(); ++j) {
        if (i != j) {
          gates_inputs_.AppendToLastVector(Literal(inputs[j], true));
        }
      }
      gates_type_.push_back(type | mask);
      if (lrat_proof_handler_ != nullptr) {
        gates_clauses_.Add({});
        for (const TruthTableId id : ids_for_proof) {
          gates_clauses_.AppendToLastVector(truth_tables_clauses_[id]);
        }
        for (const auto [a, b] : binary_used) {
          tmp_binary_clauses_.emplace_back(SatClause::Create({a, b}));
          gates_clauses_.AppendToLastVector(tmp_binary_clauses_.back().get());
        }
      }

      // Canonicalize right away to deal with corner case.
      CanonicalizeShortGate(new_id);

      if (num_bits != 2) break;
      if (mask == not_important) break;
      if (not_important == 0b1111) break;

      // If some position do not matter, for now we add "duplicate" versions
      // for all the other possibilities. At most x8 here, but hopefully this
      // should be rare.
      //
      // TODO(user): Better design is probably to incorporate table2 info as
      // we canonicalize tables...
      mask = ((mask | ~not_important) + 1) & not_important;
    }
  }
  return num_detected;
}

namespace {

// Return a BooleanVariable from b that is not in a or kNoBooleanVariable;
BooleanVariable FindMissing(absl::Span<const BooleanVariable> vars_a,
                            absl::Span<const BooleanVariable> vars_b) {
  for (const BooleanVariable b : vars_b) {
    bool found = false;
    for (const BooleanVariable a : vars_a) {
      if (a == b) {
        found = true;
        break;
      }
    }
    if (!found) return b;
  }
  return kNoBooleanVariable;
}

}  // namespace

// TODO(user): It should be possible to extract ALL possible short gates, but
// we are not there yet.
void GateCongruenceClosure::ExtractShortGates(PresolveTimer& timer) {
  if (lrat_proof_handler_ != nullptr) {
    truth_tables_clauses_.ResetFromFlatMapping(
        tmp_ids_, tmp_clauses_,
        /*minimum_num_nodes=*/truth_tables_bitset_.size());
    CHECK_EQ(truth_tables_bitset_.size(), truth_tables_clauses_.size());
  }

  // This is used to combine two 3 arity tables into one 4 arity one if they
  // share two variables.
  absl::flat_hash_map<std::array<BooleanVariable, 2>, int> binary_index_map;
  std::vector<int> flat_binary_index;
  std::vector<TruthTableId> flat_table_id;

  // Counters.
  // We only fill a subset of the entries, but that make the code shorter.
  std::vector<int> num_tables(6);

  // Note that using the indirection via TruthTableId allow this code to
  // be deterministic.
  CHECK_EQ(truth_tables_bitset_.size(), truth_tables_inputs_.size());

  // Given a table of arity 4, this merges all the information from the tables
  // of arity 3 included in it.
  int num_merges = 0;
  const auto merge3_into_4 = [this, &num_merges](
                                 absl::Span<const BooleanVariable> inputs,
                                 SmallBitset& truth_table,
                                 std::vector<TruthTableId>& ids_for_proof) {
    DCHECK_EQ(inputs.size(), 4);
    for (int i_to_remove = 0; i_to_remove < inputs.size(); ++i_to_remove) {
      int pos = 0;
      std::array<BooleanVariable, 3> key3;
      for (int i = 0; i < inputs.size(); ++i) {
        if (i == i_to_remove) continue;
        key3[pos++] = inputs[i];
      }
      const auto it = ids3_.find(key3);
      if (it == ids3_.end()) continue;
      ++num_merges;

      // Extend the bitset from the table3 so that it is expressed correctly
      // for the given inputs.
      const TruthTableId id3 = it->second;
      std::array<BooleanVariable, 4> key4;
      for (int i = 0; i < 3; ++i) key4[i] = key3[i];
      key4[3] = FindMissing(key3, inputs);
      SmallBitset bitset = truth_tables_bitset_[id3];
      bitset |= bitset << (1 << 3);  // Extend for a new variable.
      CanonicalizeTruthTable<BooleanVariable>(absl::MakeSpan(key4), bitset);
      CHECK_EQ(inputs, absl::MakeSpan(key4));
      truth_table &= bitset;

      // We need to add the corresponding clause!
      if (lrat_proof_handler_ != nullptr) {
        ids_for_proof.push_back(id3);
      }
    }
  };

  int num_merges2 = 0;
  const auto merge2_into_n =
      [this, &num_merges2](
          absl::Span<const BooleanVariable> inputs, SmallBitset& truth_table,
          std::vector<std::pair<Literal, Literal>>& binary_used) {
        for (int i = 0; i < inputs.size(); ++i) {
          for (int j = i + 1; j < inputs.size(); ++j) {
            std::array<BooleanVariable, 2> key2 = {inputs[i], inputs[j]};
            const auto it = ids2_.find(key2);
            if (it == ids2_.end()) continue;

            const SmallBitset bitset2 = it->second;
            SmallBitset bitset = bitset2;
            int new_size = 0;
            std::vector<BooleanVariable> key(inputs.size());
            key[new_size++] = inputs[i];
            key[new_size++] = inputs[j];
            for (int t = 0; t < inputs.size(); ++t) {
              if (t != i && t != j) {
                key[new_size] = inputs[t];
                bitset |= bitset << (1 << new_size);  // EXTEND
                ++new_size;
              }
            }
            CanonicalizeTruthTable<BooleanVariable>(absl::MakeSpan(key),
                                                    bitset);
            CHECK_EQ(inputs, absl::MakeSpan(key));

            const SmallBitset old = truth_table;
            truth_table &= bitset;
            if (old != truth_table) {
              AppendBinaryClausesFromTruthTable(key2, bitset2, &binary_used);
              ++num_merges2;
            }
          }
        }
      };

  // Starts by processing all existing tables.
  //
  // TODO(user): Since we deal with and_gates differently, do we need to look at
  // binary clauses here ? or that is not needed ? I think there is only two
  // kind of Boolean function on two inputs (and_gates, with any possible
  // negation) and xor_gate.
  std::vector<TruthTableId> ids_for_proof;
  std::vector<std::pair<Literal, Literal>> binary_used;
  for (TruthTableId t_id(0); t_id < truth_tables_inputs_.size(); ++t_id) {
    ids_for_proof.clear();
    ids_for_proof.push_back(t_id);
    const absl::Span<const BooleanVariable> inputs = truth_tables_inputs_[t_id];
    SmallBitset truth_table = truth_tables_bitset_[t_id];

    // Deal with fixed variables.
    for (int i = 0; i < inputs.size(); ++i) {
      const Literal lit(inputs[i], true);
      if (trail_->Assignment().LiteralIsAssigned(lit)) {
        const bool at_true = trail_->Assignment().LiteralIsTrue(lit);
        for (int p = 0; p < (1 << inputs.size()); ++p) {
          if ((p >> i) & 1) {
            // i true at position p.
            if (!at_true) truth_table &= ~(1 << p);
          } else {
            // i false at position p.
            if (at_true) truth_table &= ~(1 << p);
          }
        }
      }
    }

    // Deal with equal variable.
    // TODO(user): understand this a bit better, it only happen with
    // ProcessPreviousTruthTables() when we don't simplify the truth table.
    if (inputs.size() == 3) {
      for (int i = 0; i < inputs.size(); ++i) {
        for (int j = i + 1; j < inputs.size(); ++j) {
          if (inputs[i] == inputs[j]) {
            for (int p = 0; p < (1 << inputs.size()); ++p) {
              if (((p >> i) & 1) != ((p >> j) & 1)) {
                truth_table &= ~(1 << p);
              }
            }
          }
        }
      }
    }

    // TODO(user): it is unclear why this is useful. Understand a bit more the
    // set of possible Boolean functions of 2 and 3 variables and their clause
    // encoding.
    binary_used.clear();
    merge2_into_n(inputs, truth_table, binary_used);

    // Merge any size-3 table included inside a size-4 gate.
    // TODO(user): do it for larger gate too ?
    if (inputs.size() == 4) {
      merge3_into_4(inputs, truth_table, ids_for_proof);
    }

    // Write back the most recent data.
    truth_tables_bitset_[t_id] = truth_table;

    // Only for investigating inefficiency: if truth_table is not included in
    // old_truth_tables_bitset_[t_id], the we lost some information somewhere.
    if (VLOG_IS_ON(3) && t_id < old_truth_tables_bitset_.size() &&
        (truth_table & ~old_truth_tables_bitset_[t_id])) {
      std::vector<Literal> tmp;
      for (const BooleanVariable var : inputs) {
        tmp.push_back(Literal(var, true));
      }
      LOG(INFO) << "Lost info " << tmp << " " << std::bitset<8>(truth_table)
                << " " << std::bitset<8>(old_truth_tables_bitset_[t_id]);
    }

    ++num_tables[inputs.size()];
    const int num_detected =
        ProcessTruthTable(inputs, truth_table, ids_for_proof, binary_used);

    // If this is not a function and of size 3, lets try to combine it with
    // another truth table of size 3 to get a table of size 4.
    if (inputs.size() == 3 && num_detected == 0) {
      // Skip entry with duplicates, since we are sorted, no need to test
      // position 0 and 2. This can only happen via
      // ProcessPreviousTruthTables().
      if (inputs[0] == inputs[1]) continue;
      if (inputs[1] == inputs[2]) continue;

      for (int i = 0; i < 3; ++i) {
        std::array<BooleanVariable, 2> key{inputs[i != 0 ? 0 : 1],
                                           inputs[i != 2 ? 2 : 1]};
        DCHECK(std::is_sorted(key.begin(), key.end()));
        const auto [it, inserted] =
            binary_index_map.insert({key, binary_index_map.size()});
        flat_binary_index.push_back(it->second);
        flat_table_id.push_back(t_id);
      }
    }
  }
  gtl::STLClearObject(&binary_index_map);

  // Detects ITE gates and potentially other 3-gates from a truth table of
  // 4-entries formed by two 3-entries table. This just create a 4-entries
  // table that will be processed below.
  CompactVectorVector<int, TruthTableId> candidates;
  candidates.ResetFromFlatMapping(flat_binary_index, flat_table_id);
  gtl::STLClearObject(&flat_binary_index);
  gtl::STLClearObject(&flat_table_id);
  int num_combinations = 0;
  for (int c = 0; c < candidates.size(); ++c) {
    if (candidates[c].size() < 2) continue;
    if (candidates[c].size() > 10) continue;  // Too many? use heuristic.

    for (int a = 0; a < candidates[c].size(); ++a) {
      for (int b = a + 1; b < candidates[c].size(); ++b) {
        const absl::Span<const BooleanVariable> inputs_a =
            truth_tables_inputs_[candidates[c][a]];
        const absl::Span<const BooleanVariable> inputs_b =
            truth_tables_inputs_[candidates[c][b]];

        std::array<BooleanVariable, 4> key;
        for (int i = 0; i < 3; ++i) key[i] = inputs_a[i];
        key[3] = FindMissing(inputs_a, inputs_b);
        CHECK_NE(key[3], kNoBooleanVariable);

        // Add an all allowed entry.
        SmallBitset bitmask = GetNumBitsAtOne(1 << 4);
        CanonicalizeTruthTable<BooleanVariable>(absl::MakeSpan(key), bitmask);

        // If the key was not processed before, process it now.
        // Note that an old version created a TruthTableId for it, but that
        // waste a lot of space.
        //
        // On another hand, it is possible we process the same key up to
        // 4_choose_2 times, but this is rare...
        if (!ids4_.contains(key)) {
          ++num_combinations;
          ++num_tables[4];
          ids_for_proof.clear();
          binary_used.clear();
          merge2_into_n(key, bitmask, binary_used);
          merge3_into_4(key, bitmask, ids_for_proof);
          ProcessTruthTable(key, bitmask, ids_for_proof, binary_used);
        }
      }
    }
  }

  timer.AddCounter("combine3", num_combinations);
  timer.AddCounter("merges", num_merges);
  timer.AddCounter("merges2", num_merges2);

  // Note that we only display non-zero counters.
  for (int i = 0; i < num_tables.size(); ++i) {
    timer.AddCounter(absl::StrCat("t", i), num_tables[i]);
  }

  std::vector<int> num_functions(6);
  for (GateId id(0); id < gates_inputs_.size(); ++id) {
    const int size = gates_inputs_[id].size();
    if (size < num_functions.size()) {
      num_functions[size]++;
    }
  }
  for (int i = 0; i < num_functions.size(); ++i) {
    timer.AddCounter(absl::StrCat("fn", i), num_functions[i]);
  }
}

namespace {
// Helper class to add LRAT proofs for equivalent gate target literals.
class LratGateCongruenceHelper {
 public:
  LratGateCongruenceHelper(
      const Trail* trail, ClauseManager* clause_manager,
      LratProofHandler* lrat_proof_handler,
      const util_intops::StrongVector<GateId, Literal>& gates_target,
      const CompactVectorVector<GateId, const SatClause*>& gates_clauses,
      DenseConnectedComponentsFinder& union_find)
      : trail_(trail),
        clause_manager_(clause_manager),
        lrat_proof_handler_(lrat_proof_handler),
        gates_target_(gates_target),
        gates_clauses_(gates_clauses),
        union_find_(union_find) {}

  // Adds direct LRAT equivalence clauses between l and its representative r, as
  // well as between each of its ancestor and r. Does nothing if r is equal to l
  // or its parent. This must be called before union_find_.FindRoot(l).
  void ShortenEquivalencesWithRepresentative(Literal l) {
    std::vector<Literal> literals;
    Literal representative;
    // Append l and its ancestors, excluding the representative, to `literals`.
    while (true) {
      if (IsRepresentative(l)) {
        representative = l;
        break;
      }
      literals.push_back(l);
      l = GetParent(l);
    }

    // Add a direct equivalence between each literal in `literals` and
    // `representative` (except the last one, which already has a direct
    // equivalence). This is done in reverse order so that the proof for each
    // equivalence can use the one for the parent.
    for (int i = literals.size() - 2; i >= 0; --i) {
      const Literal parent = literals[i + 1];
      const Literal child = literals[i];

      const ClausePtr rep_implies_child =
          ClausePtr(representative.Negated(), child);
      lrat_proof_handler_->AddInferredClause(
          rep_implies_child, {ClausePtr(representative.Negated(), parent),
                              ClausePtr(parent.Negated(), child)});
      const ClausePtr child_implies_rep =
          ClausePtr(child.Negated(), representative);
      lrat_proof_handler_->AddInferredClause(
          child_implies_rep, {ClausePtr(child.Negated(), parent),
                              ClausePtr(parent.Negated(), representative)});
    }
    if (!literals.empty()) {
      // Make sure the parent links in union_find_ are shorten too, to keep the
      // consistency between the two data structures.
      union_find_.FindRoot(literals[0].Index().value());
    }
  }

  // Returns an LRAT clause rep(gates_target[gate_a_id]) =>
  // rep(gates_target[gate_b_id]).
  ClausePtr AddAndGateTargetImplication(GateId gate_a_id, GateId gate_b_id) {
    const Literal a = gates_target_[gate_a_id];
    const Literal b = gates_target_[gate_b_id];
    const Literal rep_a = GetRepresentativeWithProofSupport(a);
    const Literal rep_b = GetRepresentativeWithProofSupport(b);
    DCHECK(IsRepresentative(rep_a));
    DCHECK(IsRepresentative(rep_b));
    // Compute a sequence of clauses proving that rep(a) => rep(b).
    // The following only works for and gates.
    std::vector<ClausePtr> proof;
    // rep(a) => a:
    Append(proof, GetRepresentativeImpliesLiteralClause(a));
    // For each original input l of gate_a_id, a => l => rep(l). The original
    // inputs are the negation of each clause literal other than the target.
    // TODO(user): this can add redundant clauses if two original inputs
    // have the same representative.
    for (const Literal lit : gates_clauses_[gate_a_id][0]->AsSpan()) {
      if (lit == a) continue;
      const Literal l = lit.Negated();
      proof.push_back(ClausePtr(a.Negated(), l));
      Append(proof, GetLiteralImpliesRepresentativeClause(l));
    }
    // For each original input l of b, rep(l) => l. The original inputs are
    // the negation of each gate clause literal other than its target b.
    for (const Literal lit : gates_clauses_[gate_b_id][0]->AsSpan()) {
      if (lit == b) continue;
      const Literal l = lit.Negated();
      Append(proof, GetRepresentativeImpliesLiteralClause(l));
    }
    // The original inputs of gate_b_id imply its target b:
    proof.push_back(ClausePtr(gates_clauses_[gate_b_id][0]));
    // b => rep(b):
    Append(proof, GetLiteralImpliesRepresentativeClause(b));

    if (rep_a.Negated() == rep_b) {
      lrat_proof_handler_->AddInferredClause(ClausePtr(rep_a.Negated()), proof);
      return ClausePtr(rep_a.Negated());
    }
    const ClausePtr clause = ClausePtr(rep_a.Negated(), rep_b);
    lrat_proof_handler_->AddInferredClause(clause, proof);
    return clause;
  }

  void ClearTemporaryProof() {
    CHECK(lrat_proof_handler_ != nullptr);
    tmp_index_to_delete_.clear();
    tmp_proof_clauses_.clear();
    marked_.ClearAndResize(LiteralIndex(clause_manager_->literal_size()));
  }

  Literal GetRepresentativeWithProofSupport(Literal lit) {
    const int lit_index_as_int = lit.Index().value();
    if (union_find_.GetParent(lit_index_as_int) == lit_index_as_int) {
      return lit;
    }
    if (lrat_proof_handler_ != nullptr) {
      ShortenEquivalencesWithRepresentative(lit);
    }
    return Literal(LiteralIndex(union_find_.FindRoot(lit_index_as_int)));
  }

  // TODO(user): There is probably no need to add clauses that involve variables
  // that are no longer inputs of that gate. If for instance we showed that the
  // gate output is independent of one of the variable, it could still appear in
  // the clauses.
  void AddGateClausesToTemporaryProof(GateId id) {
    CHECK(lrat_proof_handler_ != nullptr);
    const auto& assignment = trail_->Assignment();

    for (const SatClause* clause : gates_clauses_[id]) {
      // We rewrite each clause using new equivalences or fixed literals found.
      marked_.ResetAllToFalse();
      tmp_literals_.clear();
      tmp_proof_.clear();
      bool clause_is_trivial = false;
      bool some_change = false;
      for (const Literal lit : clause->AsSpan()) {
        const Literal rep = GetRepresentativeWithProofSupport(lit);

        if (assignment.LiteralIsAssigned(rep)) {
          if (assignment.LiteralIsTrue(rep)) {
            clause_is_trivial = true;
            break;  // Not needed.
          } else {
            some_change = true;
            tmp_proof_.push_back(ClausePtr(rep.Negated()));
            if (rep != lit) {
              tmp_proof_.push_back(GetLiteralImpliesRepresentativeClause(lit));
            }
            continue;
          }
        }

        if (rep != lit) {
          some_change = true;
          // We need not(rep) => not(lit). This should be equivalent to
          // getting lit => rep. Both should work.
          tmp_proof_.push_back(GetLiteralImpliesRepresentativeClause(lit));
        }

        if (marked_[rep]) continue;
        if (marked_[rep.Negated()]) {
          clause_is_trivial = true;
          break;
        }
        marked_.Set(rep);
        tmp_literals_.push_back(rep);
      }

      // If this is the case, we shouldn't need it for the proof.
      if (clause_is_trivial) continue;

      ClausePtr new_clause =
          clause->size() == 2
              ? ClausePtr(clause->FirstLiteral(), clause->SecondLiteral())
              : ClausePtr(clause);
      if (some_change) {
        // If there is some change, we add a temporary clause with the proof to
        // go from the original clause to this one.
        tmp_proof_.push_back(new_clause);
        new_clause = NewClausePtr(tmp_literals_);
        lrat_proof_handler_->AddInferredClause(new_clause, tmp_proof_);
        if (new_clause.IsSatClausePtr()) {
          tmp_index_to_delete_.push_back(tmp_proof_clauses_.size());
        }
      }

      // Add that clause to the set of clauses needed for the proof.
      tmp_proof_clauses_.push_back(new_clause);
    }
  }

  // Same as AddAndGateTargetImplication() but with truth table based gates.
  std::pair<ClausePtr, ClausePtr> AddShortGateEquivalence(
      Literal rep_a, Literal rep_b, absl::Span<const GateId> gate_ids) {
    if (lrat_proof_handler_ == nullptr) return {kNullClausePtr, kNullClausePtr};

    // Just add all clauses from both gates.
    // But note that we need to remap them.
    ClearTemporaryProof();
    for (const GateId id : gate_ids) {
      AddGateClausesToTemporaryProof(id);
    }

    // Corner case: If rep_a or rep_b are fixed, add proof for that.
    for (const Literal lit : {rep_a, rep_b}) {
      const auto& assignment = trail_->Assignment();
      if (assignment.LiteralIsAssigned(lit)) {
        const Literal true_lit =
            assignment.LiteralIsTrue(lit) ? lit : lit.Negated();
        tmp_proof_clauses_.push_back(ClausePtr(true_lit));
      }
    }

    // All clauses are now in tmp_proof_clauses_.
    // We can add both implications with proof.
    DCHECK(IsRepresentative(rep_a));
    DCHECK(IsRepresentative(rep_b));
    CHECK_NE(rep_a, rep_b);

    ClausePtr rep_a_implies_rep_b;
    ClausePtr rep_b_implies_rep_a;
    if (rep_a.Negated() == rep_b) {
      // The model is UNSAT.
      //
      // TODO(user): AddAndProveInferredClauseByEnumeration() do not like
      // duplicates, but maybe we should just handle the case to have
      // less code here?
      rep_a_implies_rep_b = ClausePtr(rep_a.Negated());
      rep_b_implies_rep_a = ClausePtr(rep_a);
      lrat_proof_handler_->AddAndProveInferredClauseByEnumeration(
          rep_a_implies_rep_b, tmp_proof_clauses_);
      lrat_proof_handler_->AddAndProveInferredClauseByEnumeration(
          rep_b_implies_rep_a, tmp_proof_clauses_);
    } else {
      rep_a_implies_rep_b = ClausePtr(rep_a.Negated(), rep_b);
      rep_b_implies_rep_a = ClausePtr(rep_b.Negated(), rep_a);
      lrat_proof_handler_->AddAndProveInferredClauseByEnumeration(
          rep_a_implies_rep_b, tmp_proof_clauses_);
      lrat_proof_handler_->AddAndProveInferredClauseByEnumeration(
          rep_b_implies_rep_a, tmp_proof_clauses_);
    }

    for (const int i : tmp_index_to_delete_) {
      lrat_proof_handler_->DeleteClause(tmp_proof_clauses_[i]);
    }
    return {rep_a_implies_rep_b, rep_b_implies_rep_a};
  }

  ClausePtr ProofForFixingLiteral(Literal to_fix, GateId id) {
    if (lrat_proof_handler_ == nullptr) return kNullClausePtr;
    CHECK(IsRepresentative(to_fix));
    ClearTemporaryProof();
    AddGateClausesToTemporaryProof(id);
    const ClausePtr to_fix_clause = ClausePtr(to_fix);
    lrat_proof_handler_->AddAndProveInferredClauseByEnumeration(
        to_fix_clause, tmp_proof_clauses_);

    for (const int i : tmp_index_to_delete_) {
      lrat_proof_handler_->DeleteClause(tmp_proof_clauses_[i]);
    }
    return to_fix_clause;
  }

  // Appends to `proof` the clauses "gates_target[gate_id] => l => rep" and
  // "gates_target[gate_id] => m => not(rep)", proving that the gate target
  // literal can be fixed to false (assuming this is an and gate). A
  // precondition is that two original inputs l and m with rep(l) = rep and
  // rep(m) = not(rep) must exist.
  void AppendFixAndGateTargetClauses(GateId gate_id, Literal rep,
                                     absl::InlinedVector<ClausePtr, 5>& proof) {
    const Literal target = gates_target_[gate_id];
    LiteralIndex l_index = kNoLiteralIndex;
    LiteralIndex m_index = kNoLiteralIndex;
    // Find l and m in the original inputs (the negation of each gate clause
    // literal other than its target).
    for (const Literal lit : gates_clauses_[gate_id][0]->AsSpan()) {
      if (l_index != kNoLiteralIndex && m_index != kNoLiteralIndex) break;
      const Literal l = lit.Negated();
      const Literal rep_l = GetRepresentativeWithProofSupport(l);
      if (rep_l == rep) l_index = l.Index();
      if (rep_l == rep.Negated()) m_index = l.Index();
    }
    CHECK(l_index != kNoLiteralIndex && m_index != kNoLiteralIndex);

    // We want to prove target_rep.Negated(), we start by showing that
    // target_rep implies target.
    Append(proof, GetRepresentativeImpliesLiteralClause(target));
    proof.push_back(ClausePtr(target.Negated(), Literal(l_index)));
    Append(proof, GetLiteralImpliesRepresentativeClause(Literal(l_index)));
    proof.push_back(ClausePtr(target.Negated(), Literal(m_index)));
    Append(proof, GetLiteralImpliesRepresentativeClause(Literal(m_index)));
  }

 private:
  // The IDs of the two implications of an equivalence between two gate targets.
  struct GateEquivalenceClauses {
    ClausePtr parent_implies_child;
    ClausePtr child_implies_parent;
  };

  bool IsRepresentative(Literal l) const { return GetParent(l) == l; }

  Literal GetParent(Literal l) const {
    return Literal(LiteralIndex(union_find_.GetParent(l.Index().value())));
  }

  ClausePtr GetLiteralImpliesRepresentativeClause(Literal l) {
    ShortenEquivalencesWithRepresentative(l);
    const Literal rep = GetParent(l);
    return rep != l ? ClausePtr(l.Negated(), rep) : kNullClausePtr;
  }

  ClausePtr GetRepresentativeImpliesLiteralClause(Literal l) {
    ShortenEquivalencesWithRepresentative(l);
    const Literal rep = GetParent(l);
    return rep != l ? ClausePtr(rep.Negated(), l) : kNullClausePtr;
  }

  template <typename Vector>
  static void Append(Vector& clauses, ClausePtr new_clause) {
    if (new_clause != kNullClausePtr) {
      clauses.push_back(new_clause);
    }
  }

  const Trail* trail_;
  ClauseManager* clause_manager_;
  LratProofHandler* lrat_proof_handler_;
  const util_intops::StrongVector<GateId, Literal>& gates_target_;
  const CompactVectorVector<GateId, const SatClause*>& gates_clauses_;
  DenseConnectedComponentsFinder& union_find_;

  // For AddShortGateTargetImplication().
  std::vector<int> tmp_index_to_delete_;
  std::vector<ClausePtr> tmp_proof_clauses_;

  // For the simplification of clauses using equivalences in
  // AddGateClausesToTemporaryProof().
  SparseBitset<LiteralIndex> marked_;
  std::vector<ClausePtr> tmp_proof_;
  std::vector<Literal> tmp_literals_;
};

}  // namespace

std::string GateCongruenceClosure::GateDebugString(GateId id) const {
  std::string result;
  absl::StrAppend(&result, "[");
  for (int i = 0; i < gates_inputs_[id].size(); ++i) {
    const Literal lit = gates_inputs_[id][i];
    absl::StrAppend(&result, (i == 0 ? "" : ","), lit, " ",
                    assignment_.LiteralIsTrue(lit)    ? "T"
                    : assignment_.LiteralIsFalse(lit) ? "F"
                                                      : "?");
  }
  absl::StrAppend(&result, "]");
  const Literal target(gates_target_[id]);
  absl::StrAppend(&result, " target ", target);
  absl::StrAppend(&result, " ",
                  assignment_.LiteralIsTrue(target)    ? "T"
                  : assignment_.LiteralIsFalse(target) ? "F"
                                                       : "?");
  absl::StrAppend(&result, " ", std::bitset<8>(gates_type_[id]).to_string());
  return result;
}

void GateCongruenceClosure::StructureExtraction(PresolveTimer& timer) {
  const int num_variables(sat_solver_->NumVariables());
  const int num_literals(num_variables * 2);
  marked_.ClearAndResize(Literal(num_literals));
  seen_.ClearAndResize(Literal(num_literals));
  next_seen_.ClearAndResize(Literal(num_literals));

  gates_target_.clear();
  gates_inputs_.clear();
  gates_type_.clear();
  gates_clauses_.clear();

  ExtractAndGatesAndFillShortTruthTables(timer);
  ExtractShortGates(timer);

  // All vector have the same size.
  // Except gates_clauses_ which is only filled if we need proof.
  CHECK_EQ(gates_target_.size(), gates_type_.size());
  CHECK_EQ(gates_target_.size(), gates_inputs_.size());
  if (lrat_proof_handler_ != nullptr) {
    CHECK_EQ(gates_target_.size(), gates_clauses_.size());
  }
}

bool GateCongruenceClosure::DoOneRound(bool log_info) {
  // TODO(user): Remove this condition, it is possible there are no binary
  // and still gates!
  if (implication_graph_->IsEmpty()) return true;

  // We will fully propagate newly fixed variables.
  CHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  clause_manager_->AttachAllClauses();

  PresolveTimer timer("GateCongruenceClosure", logger_, time_limit_);
  timer.OverrideLogging(log_info || VLOG_IS_ON(2));

  // Lets release the memory on exit.
  CHECK(tmp_binary_clauses_.empty());
  absl::Cleanup binary_cleanup = [this] {
    tmp_binary_clauses_.clear();
    if (lrat_proof_handler_ != nullptr) {
      lrat_proof_handler_->DeleteTemporaryBinaryClauses();
    }
  };

  const int num_variables(sat_solver_->NumVariables());
  const int num_literals(num_variables * 2);
  StructureExtraction(timer);

  // If two gates have the same type and the same inputs, their targets are
  // equivalent. We use an hash set to detect that the inputs are the same.
  absl::flat_hash_set<GateId, GateHash, GateEq> gate_set(
      /*capacity=*/gates_inputs_.size(), GateHash(&gates_type_, &gates_inputs_),
      GateEq(&gates_type_, &gates_inputs_));

  // Used to find representatives as we detect equivalent literal.
  DenseConnectedComponentsFinder union_find;
  union_find.SetNumberOfNodes(num_literals);

  // This will also be updated as we detect equivalences and allows to find
  // all the gates with a given input literal, taking into account all its
  // equivalences.
  //
  // Tricky: we need to resize this to num_literals because the union_find that
  // merges target can choose for a representative a literal that is not in the
  // set of gate inputs.
  MergeableOccurrenceList<BooleanVariable, GateId> input_var_to_gate;
  struct GetVarMapper {
    BooleanVariable operator()(Literal l) const { return l.Variable(); }
  };
  input_var_to_gate.ResetFromTransposeMap<GetVarMapper>(gates_inputs_,
                                                        num_variables);

  LratGateCongruenceHelper lrat_helper(trail_, clause_manager_,
                                       lrat_proof_handler_, gates_target_,
                                       gates_clauses_, union_find);

  // Stats + make sure we run it at exit.
  int num_units = 0;
  int num_equivalences = 0;
  int num_processed = 0;
  int arity1_equivalences = 0;
  absl::Cleanup stat_cleanup = [&] {
    total_wtime_ += timer.wtime();
    total_dtime_ += timer.deterministic_time();
    total_equivalences_ += num_equivalences;
    total_num_units_ += num_units;
    timer.AddCounter("processed", num_processed);
    timer.AddCounter("units", num_units);
    timer.AddCounter("f1_equiv", arity1_equivalences);
    timer.AddCounter("equiv", num_equivalences);
  };

  // Starts with all gates in the queue.
  const int num_gates = gates_inputs_.size();
  total_gates_ += num_gates;
  std::vector<bool> in_queue(num_gates, true);
  std::vector<GateId> queue(num_gates);
  for (GateId id(0); id < num_gates; ++id) queue[id.value()] = id;

  int num_processed_fixed_variables = trail_->Index();

  const auto fix_literal = [&, this](Literal to_fix,
                                     absl::Span<const ClausePtr> proof) {
    DCHECK_EQ(to_fix, lrat_helper.GetRepresentativeWithProofSupport(to_fix));
    if (assignment_.LiteralIsTrue(to_fix)) return true;
    if (!clause_manager_->InprocessingFixLiteral(to_fix, proof)) return false;

    // Also propagate all the clauses.
    // TODO(user): We might not want to trigger LP or costly propagator here.
    if (!sat_solver_->FinishPropagation()) return false;

    // This is quite tricky: as we fix a literal, we propagate right away
    // everything implied by it in the binary implication graph. So we need to
    // loop over all newly_fixed variable in order to properly reach the fix
    // point!
    ++num_units;
    for (; num_processed_fixed_variables < trail_->Index();
         ++num_processed_fixed_variables) {
      const Literal to_update = lrat_helper.GetRepresentativeWithProofSupport(
          (*trail_)[num_processed_fixed_variables]);
      for (const GateId gate_id : input_var_to_gate[to_update.Variable()]) {
        if (in_queue[gate_id.value()]) continue;
        queue.push_back(gate_id);
        in_queue[gate_id.value()] = true;
      }
      input_var_to_gate.ClearList(to_update.Variable());
    }
    return true;
  };

  const auto new_equivalence = [&, this](Literal a, Literal b,
                                         ClausePtr a_implies_b,
                                         ClausePtr b_implies_a) {
    if (a == b.Negated()) {
      // The model is UNSAT.
      if (lrat_proof_handler_ != nullptr) {
        DCHECK_EQ(a_implies_b, ClausePtr(a.Negated()));
        DCHECK_EQ(b_implies_a, ClausePtr(a));
        lrat_proof_handler_->AddInferredClause(ClausePtr::EmptyClausePtr(),
                                               {a_implies_b, b_implies_a});
      }
      return false;
    }
    // Lets propagate fixed variables as we find new equivalences.
    if (assignment_.LiteralIsAssigned(a)) {
      if (assignment_.LiteralIsTrue(a)) {
        return fix_literal(b, {a_implies_b, ClausePtr(a)});
      } else {
        return fix_literal(b.Negated(), {b_implies_a, ClausePtr(a.Negated())});
      }
    } else if (assignment_.LiteralIsAssigned(b)) {
      if (assignment_.LiteralIsTrue(b)) {
        return fix_literal(a, {b_implies_a, ClausePtr(b)});
      } else {
        return fix_literal(a.Negated(), {a_implies_b, ClausePtr(b.Negated())});
      }
    }
    if (lrat_proof_handler_ != nullptr) {
      DCHECK_EQ(a_implies_b, ClausePtr(a.Negated(), b));
      DCHECK_EQ(b_implies_a, ClausePtr(b.Negated(), a));
    }

    ++num_equivalences;
    DCHECK(!implication_graph_->IsRedundant(a));
    DCHECK(!implication_graph_->IsRedundant(b));
    if (!implication_graph_->AddBinaryClause(a.Negated(), b) ||
        !implication_graph_->AddBinaryClause(b.Negated(), a)) {
      return false;
    }

    BooleanVariable to_merge_var = kNoBooleanVariable;
    BooleanVariable rep_var = kNoBooleanVariable;
    for (const bool negate : {false, true}) {
      const LiteralIndex x = negate ? a.NegatedIndex() : a.Index();
      const LiteralIndex y = negate ? b.NegatedIndex() : b.Index();

      // Because x always refer to a and y to b, this should maintain
      // the invariant root(lit) = root(lit.Negated()).Negated().
      // This is checked below.
      union_find.AddEdge(x.value(), y.value());
      const LiteralIndex rep(union_find.FindRoot(y.value()));
      const LiteralIndex to_merge = rep == x ? y : x;
      if (to_merge_var == kNoBooleanVariable) {
        to_merge_var = Literal(to_merge).Variable();
        rep_var = Literal(rep).Variable();
      } else {
        // We should have the same var.
        CHECK_EQ(to_merge_var, Literal(to_merge).Variable());
        CHECK_EQ(rep_var, Literal(rep).Variable());
      }
    }

    // Invariant.
    CHECK_EQ(
        lrat_helper.GetRepresentativeWithProofSupport(a),
        lrat_helper.GetRepresentativeWithProofSupport(a.Negated()).Negated());
    CHECK_EQ(
        lrat_helper.GetRepresentativeWithProofSupport(b),
        lrat_helper.GetRepresentativeWithProofSupport(b.Negated()).Negated());

    // Re-add to the queue all gates with touched inputs.
    //
    // TODO(user): I think we could only add the gates of "to_merge"
    // before we merge. This part of the code is quite quick in any
    // case.
    input_var_to_gate.MergeInto(to_merge_var, rep_var);
    for (const GateId gate_id : input_var_to_gate[rep_var]) {
      if (in_queue[gate_id.value()]) continue;
      queue.push_back(gate_id);
      in_queue[gate_id.value()] = true;
    }

    return true;
  };

  // Main loop.
  while (!queue.empty()) {
    ++num_processed;
    const GateId id = queue.back();
    queue.pop_back();
    CHECK(in_queue[id.value()]);
    in_queue[id.value()] = false;

    // Tricky: the hash-map might contain id not yet canonicalized. And in
    // particular might already contain the id we are processing.
    //
    // The first pass will check equivalence with the "non-canonical
    // version" and remove id if it was already there. The second will do it on
    // the canonicalized version.
    for (int pass = 0; pass < 2; ++pass) {
      GateId other_id(-1);
      bool is_equivalent = false;
      if (pass == 0) {
        const auto it = gate_set.find(id);
        if (it != gate_set.end()) {
          other_id = *it;
          if (id == other_id) {
            // This gate was already in the set, remove it before we
            // insert its potentially different canonicalized version.
            gate_set.erase(it);
          } else {
            is_equivalent = true;
          }
        }
      } else {
        const auto [it, inserted] = gate_set.insert(id);
        if (!inserted) {
          other_id = *it;
          is_equivalent = true;
        }
      }

      if (is_equivalent) {
        CHECK_NE(id, other_id);
        CHECK_GE(other_id, 0);
        CHECK_EQ(gates_type_[id], gates_type_[other_id]);
        CHECK_EQ(gates_inputs_[id], gates_inputs_[other_id]);

        input_var_to_gate.RemoveFromFutureOutput(id);

        // We detected a <=> b (or, equivalently, rep(a) <=> rep(b)).
        const Literal a(gates_target_[id]);
        const Literal b(gates_target_[other_id]);
        const Literal rep_a = lrat_helper.GetRepresentativeWithProofSupport(a);
        const Literal rep_b = lrat_helper.GetRepresentativeWithProofSupport(b);
        if (rep_a != rep_b) {
          ClausePtr rep_a_implies_rep_b = kNullClausePtr;
          ClausePtr rep_b_implies_rep_a = kNullClausePtr;
          if (lrat_proof_handler_ != nullptr) {
            if (gates_type_[id] == kAndGateType) {
              rep_a_implies_rep_b =
                  lrat_helper.AddAndGateTargetImplication(id, other_id);
              rep_b_implies_rep_a =
                  lrat_helper.AddAndGateTargetImplication(other_id, id);
            } else {
              const auto [x, y] = lrat_helper.AddShortGateEquivalence(
                  rep_a, rep_b, {id, other_id});
              rep_a_implies_rep_b = x;
              rep_b_implies_rep_a = y;
            }
          }
          if (!new_equivalence(rep_a, rep_b, rep_a_implies_rep_b,
                               rep_b_implies_rep_a)) {
            return false;
          }
        }
        break;
      }

      // Canonicalize on pass zero, otherwise loop.
      // Note that even if nothing change, we want to run canonicalize at
      // least once on the small "truth table" gates.
      //
      // Note that sorting works for and_gate and any gate that does not depend
      // on the order of its inputs. But if we add more fancy functions, we will
      // need to be careful.
      if (pass > 0) continue;

      if (gates_type_[id] == kAndGateType) {
        absl::Span<Literal> inputs = gates_inputs_[id];
        marked_.ResetAllToFalse();
        int new_size = 0;
        bool is_unit = false;
        for (const Literal lit : inputs) {
          if (lrat_proof_handler_ != nullptr) {
            lrat_helper.ShortenEquivalencesWithRepresentative(lit);
          }
          const LiteralIndex rep(union_find.FindRoot(lit.Index().value()));
          if (marked_[rep]) continue;

          // This only works for and-gate, if both lit and not(lit) are input,
          // then target must be false.
          if (marked_[Literal(rep).Negated()]) {
            is_unit = true;
            input_var_to_gate.RemoveFromFutureOutput(id);

            const Literal initial_to_fix = gates_target_[id].Negated();
            const Literal to_fix =
                lrat_helper.GetRepresentativeWithProofSupport(initial_to_fix);
            if (!assignment_.LiteralIsTrue(to_fix)) {
              absl::InlinedVector<ClausePtr, 5> proof;
              if (lrat_proof_handler_ != nullptr) {
                lrat_helper.AppendFixAndGateTargetClauses(id, Literal(rep),
                                                          proof);
              }
              if (!fix_literal(to_fix, proof)) return false;
            }
            break;
          }

          marked_.Set(rep);
          inputs[new_size++] = Literal(rep);
        }

        if (is_unit) {
          break;  // Abort the passes loop.
        }

        // We need to re-sort, even if the new_size is the same, since
        // representatives might be different.
        CHECK_GT(new_size, 0);
        std::sort(inputs.begin(), inputs.begin() + new_size);
        gates_inputs_.Shrink(id, new_size);

        // Lets convert a kAndGateType to the short "type" if we can. The truth
        // table is simply a 1 on the last position (where all inputs are ones).
        // We fall back to the case below to canonicalize further.
        //
        // This is needed because while our generic and_gate use 1 clause +
        // binary, it won't detect a kAndGateType "badly" encoded with 4 ternary
        // clauses for instance:
        //
        // b & c           => a
        // not(b) & c      => not(a)
        // b & not(c)      => not(a)
        // not(b) & not(c) => not(a)
        //
        // This is even more important since "generic" short gates might get
        // simplified as we detect equivalences, and become an and_gate later.
        if (new_size > 4) continue;
        gates_type_[id] = 1 << ((1 << new_size) - 1);
      }

      // Generic "short" gates.
      // We just take the representative and re-canonicalize.
      DCHECK_GE(gates_type_[id], 0);
      DCHECK_EQ(gates_type_[id] >> (1 << (gates_inputs_[id].size())), 0);

      for (Literal& lit_ref : gates_inputs_[id]) {
        lit_ref = lrat_helper.GetRepresentativeWithProofSupport(lit_ref);
      }

      // Note that the test below assume canonicalization.
      const int new_size = CanonicalizeShortGate(id);
      if (new_size == 2 && gates_type_[id] == 0b0110) {
        // TODO(user): we can also have 0b0100 which correspond to a binary
        // clause worth of info (or fixing both variables).
        const Literal target(gates_target_[id]);
        if (assignment_.LiteralIsAssigned(target)) {
          // We have an equivalence between the two inputs!
          input_var_to_gate.RemoveFromFutureOutput(id);
          const Literal a = gates_inputs_[id][0];
          Literal b = gates_inputs_[id][1];
          if (assignment_.LiteralIsTrue(target)) {
            b = b.Negated();
          }
          const auto [a_to_b, b_to_a] =
              lrat_helper.AddShortGateEquivalence(a, b, {id});
          if (!new_equivalence(a, b, a_to_b, b_to_a)) {
            return false;
          }
          break;
        }
      } else if (new_size == 1) {
        // We have a function of size 1! This is an equivalence.
        CHECK_EQ(gates_type_[id], 0b10);
        input_var_to_gate.RemoveFromFutureOutput(id);
        const Literal a = gates_target_[id];
        const Literal b = gates_inputs_[id][0];
        const Literal rep_a = lrat_helper.GetRepresentativeWithProofSupport(a);
        const Literal rep_b = lrat_helper.GetRepresentativeWithProofSupport(b);
        if (rep_a != rep_b) {
          ++arity1_equivalences;
          const auto [a_to_b, b_to_a] =
              lrat_helper.AddShortGateEquivalence(rep_a, rep_b, {id});
          if (!new_equivalence(rep_a, rep_b, a_to_b, b_to_a)) {
            return false;
          }
        }
        break;
      } else if (new_size == 0) {
        // We have a fixed function! Just fix the literal.
        input_var_to_gate.RemoveFromFutureOutput(id);
        const Literal initial_to_fix = (gates_type_[id] & 1) == 1
                                           ? gates_target_[id]
                                           : gates_target_[id].Negated();
        const Literal to_fix =
            lrat_helper.GetRepresentativeWithProofSupport(initial_to_fix);
        if (!assignment_.LiteralIsTrue(to_fix)) {
          if (!fix_literal(to_fix,
                           {lrat_helper.ProofForFixingLiteral(to_fix, id)})) {
            return false;
          }
        }
        break;
      }
    }
  }

  if (DEBUG_MODE) {
    CHECK_EQ(num_processed_fixed_variables, trail_->Index());
    CHECK(queue.empty());
    gate_set.clear();
    for (GateId id(0); id < num_gates; ++id) {
      if (gates_type_[id] == kAndGateType) continue;
      if (assignment_.LiteralIsAssigned(Literal(gates_target_[id]))) continue;

      const int new_size = CanonicalizeShortGate(id);
      if (new_size == 0) {
        CHECK_EQ(gates_type_[id] & 1, 0);
        const Literal initial_to_fix = Literal(gates_target_[id]).Negated();
        const Literal to_fix =
            lrat_helper.GetRepresentativeWithProofSupport(initial_to_fix);
        CHECK(assignment_.LiteralIsTrue(to_fix));
      } else if (new_size == 1 && gates_type_[id] == 0b10) {
        CHECK(!assignment_.LiteralIsAssigned(gates_target_[id]));
        CHECK(!assignment_.LiteralIsAssigned(gates_inputs_[id][0]));
        CHECK_EQ(
            lrat_helper.GetRepresentativeWithProofSupport(gates_target_[id]),
            lrat_helper.GetRepresentativeWithProofSupport(gates_inputs_[id][0]))
            << GateDebugString(id);
      } else {
        const auto [it, inserted] = gate_set.insert(id);
        if (!inserted) {
          const GateId other_id = *it;
          CHECK_EQ(
              lrat_helper.GetRepresentativeWithProofSupport(gates_target_[id]),
              lrat_helper.GetRepresentativeWithProofSupport(
                  gates_target_[other_id]))
              << GateDebugString(id) << "\n"
              << GateDebugString(other_id);
        }
      }
    }
  }

  return true;
}

}  // namespace sat
}  // namespace operations_research
