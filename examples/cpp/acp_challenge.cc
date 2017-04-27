// Copyright 2010-2014 Google
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
// ACP 2014 challenge

#include <cstdio>

#include "base/commandlineflags.h"
#include "base/file.h"
#include "base/hash.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/split.h"
#include "base/map_util.h"
#include "base/stringprintf.h"
#include "base/strtoint.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/routing.h"
#include "util/tuple_set.h"
#include "util/filelineiter.h"

/* Data format
15
8
0 0 0 0 0 0 0 0 1 0 0 0 0 0 0
0 0 0 0 0 0 0 0 0 1 0 0 1 0 0
0 0 0 0 0 0 0 0 0 0 1 0 0 0 0
0 0 0 0 0 0 0 0 0 0 0 0 0 1 0
0 0 0 0 0 0 0 0 0 1 1 0 0 0 0
0 0 0 0 0 0 0 0 0 0 1 0 0 0 1
0 0 0 0 0 0 1 0 0 0 0 0 0 0 0
0 0 0 0 0 0 0 0 0 1 0 1 0 0 0
10
  0   78   86   93  120 12 155 20
165    0  193  213  178 12  90 20
214  170    0  190  185 12  40 20
178  177  185    0  196 12 155 66
201  199  215  190    0 12 155 20
201  100   88  190   14  0  75 70
 50  44   155  190   111 12 0  20
201  199  215  190   123 70 155 0
*/

DEFINE_string(input, "", "");
DEFINE_int32(lns_size, 6, "lns size");
DEFINE_int32(lns_intervals, 4, "lns num of intervals");
DEFINE_int32(lns_seed, 0, "lns seed");
DEFINE_int32(ls_swaps, 10, "ls swaps");
DEFINE_int32(ls_rounds, 1000000, "ls rounds");
DEFINE_int32(ls_seed, 0, "ls seed");
DEFINE_int32(lns_product, 3, "lns product");
DEFINE_int32(lns_limit, 30, "Limit the number of failures of the lns loop.");
DEFINE_string(solution, "", "Solution file");
DEFINE_int32(time_limit, 0, "Time limit");
DEFINE_bool(use_lns, true, "Use LNS");
DEFINE_bool(use_filter, true, "Use LS filter");
DEFINE_bool(use_tabu, false, "Use tabu search");
DEFINE_int32(tabu_size, 10, "tabu size");
DEFINE_double(tabu_factor, 0.6, "tabu factor");
DEFINE_bool(use_sa, false, "Use simulated annealing");
DEFINE_int32(sa_temperature, 20, "Simulated annealing temperature");

DECLARE_bool(log_prefix);

namespace operations_research {

class AcpData {
 public:
  AcpData()
      : num_periods_(-1), num_products_(-1), inventory_cost_(0), state_(0) {}

  void Load(const std::string& filename) {
    for (const std::string& line : FileLines(filename)) {
      if (line.empty()) {
        continue;
      }
      ProcessNewLine(line);
    }
  }

  void ProcessNewLine(const std::string& line) {
    const std::vector<std::string> words =
        strings::Split(line, " ", strings::SkipEmpty());
    if (words.empty()) return;
    switch (state_) {
      case 0: {
        num_periods_ = atoi32(words[0]);
        state_ = 1;
        break;
      }
      case 1: {
        num_products_ = atoi32(words[0]);
        state_ = 2;
        break;
      }
      case 2: {
        due_dates_per_product_.resize(due_dates_per_product_.size() + 1);
        CHECK_EQ(words.size(), num_periods_) << "Error with line " << line;
        for (int i = 0; i < num_periods_; ++i) {
          if (atoi32(words[i]) == 1) {
            due_dates_per_product_.back().push_back(i);
          }
        }
        if (due_dates_per_product_.size() == num_products_) {
          state_ = 3;
        }
        break;
      }
      case 3: {
        inventory_cost_ = atoi32(words[0]);
        state_ = 4;
        break;
      }
      case 4: {
        transitions_.resize(transitions_.size() + 1);
        CHECK_EQ(words.size(), num_products_);
        for (int i = 0; i < num_products_; ++i) {
          transitions_.back().push_back(atoi32(words[i]));
        }
        break;
      }
      default: {
        LOG(ERROR) << "Should not be here";
      }
    }
  }

  std::string DebugString() const {
    return StringPrintf("AcpData(%d periods, %d products, %d cost)",
                        num_periods_, num_products_, inventory_cost_);
  }

  const std::vector<std::vector<int>>& due_dates_per_product() const {
    return due_dates_per_product_;
  }

  const std::vector<std::vector<int>>& transitions() const {
    return transitions_;
  }

  int num_periods() const { return num_periods_; }
  int num_products() const { return num_products_; }
  int inventory_cost() const { return inventory_cost_; }

 private:
  int num_periods_;
  int num_products_;
  int inventory_cost_;
  std::vector<std::vector<int>> due_dates_per_product_;
  std::vector<std::vector<int>> transitions_;
  int state_;
};

class RandomIntervalLns : public BaseLns {
 public:
  RandomIntervalLns(const std::vector<IntVar*>& vars,
                    const std::vector<int> item_to_product,
                    int number_of_variables, int number_of_intervals,
                    int32 seed, int num_product)
      : BaseLns(vars),
        item_to_product_(item_to_product),
        rand_(seed),
        number_of_variables_(number_of_variables),
        number_of_intervals_(number_of_intervals),
        num_product_(num_product)  {
    CHECK_GT(number_of_variables_, 0);
    CHECK_LE(number_of_variables_, Size());
    CHECK_GT(number_of_intervals_, 0);
    CHECK_LE(number_of_intervals_, Size());
  }

  ~RandomIntervalLns() {}

  virtual void InitFragments() { state_ = 0; }

  virtual bool NextFragment() {
    switch (state_) {
      case 0: {
        for (int i = 0; i < number_of_intervals_; ++i) {
          const int start = rand_.Uniform(Size() - number_of_variables_);
          for (int pos = start; pos < start + number_of_variables_; ++pos) {
            AppendToFragment(pos);
          }
        }
        break;
      }
      case 1: {
        for (int i = 0; i < number_of_variables_ * number_of_intervals_; ++i) {
          const int pos = rand_.Uniform(Size());
          AppendToFragment(pos);
        }
        break;
      }
      case 2: {
        const int length = number_of_intervals_ * number_of_variables_;
        const int start = rand_.Uniform(Size() - length);
        for (int pos = start; pos < start + length; ++pos) {
          AppendToFragment(pos);
        }
        break;
      }
      case 3: {
        std::unordered_set<int> to_release;
        while (to_release.size() < num_product_) {
          to_release.insert(rand_.Uniform(item_to_product_.back() + 1));
        }
        for (int i = 0; i < Size(); ++i) {
          if (ContainsKey(to_release, item_to_product_[Value(i)])) {
            AppendToFragment(i);
          }
        }
        break;
      }
    }
    state_++;
    state_ = state_ % 4;
    return true;
  }

  virtual std::string DebugString() const { return "RandomIntervalLns"; }

 private:
  const std::vector<int> item_to_product_;
  ACMRandom rand_;
  const int number_of_variables_;
  const int number_of_intervals_;
  const int num_product_;
  int state_;
};

class Swap : public IntVarLocalSearchOperator {
 public:
  explicit Swap(const std::vector<IntVar*>& variables)
      : IntVarLocalSearchOperator(variables),
        index1_(0),
        index2_(0) {}

  virtual ~Swap() {}

 protected:
  // Make a neighbor assigning one variable to its target value.
  virtual bool MakeOneNeighbor() {
    const int size = Size();
    index2_++;
    if (index2_ == size) {
      index1_++;
      index2_ = index1_ + 1;
    }
    if (index1_ == size - 1) {
      return false;
    }
    SetValue(index1_, OldValue(index2_));
    SetValue(index2_, OldValue(index1_));
    return true;
  }

 private:
  virtual void OnStart() {
    index1_ = 0;
    index2_ = 0;
  }

  int index1_;
  int index2_;
};

class Reverse : public IntVarLocalSearchOperator {
 public:
  explicit Reverse(const std::vector<IntVar*>& variables)
      : IntVarLocalSearchOperator(variables),
        start_(-1),
        len_(3) {}

  virtual ~Reverse() {}

 protected:
  // Make a neighbor assigning one variable to its target value.
  virtual bool MakeOneNeighbor() {
    const int size = Size();
    start_++;
    if (start_ + len_ >= size) {
      len_++;
      start_ = 0;
    }
    if (len_ >= 20) {
      return false;
    }
    for (int i = 0; i < len_; ++i) {
      SetValue(start_ + len_ - i - 1,  OldValue(start_ + i));
    }
    return true;
  }

 private:
  virtual void OnStart() {
    start_ = -1;
    len_ = 3;
  }

  int start_;
  int len_;
};

class NRandomSwaps : public IntVarLocalSearchOperator {
 public:
  explicit NRandomSwaps(const std::vector<IntVar*>& variables,
                        int num_swaps,
                        int num_rounds,
                        int ls_seed)
      : IntVarLocalSearchOperator(variables),
        num_swaps_(num_swaps),
        num_rounds_(num_rounds),
        rand_(ls_seed) {}

  virtual ~NRandomSwaps() {}

 protected:
  // Make a neighbor assigning one variable to its target value.
  virtual bool MakeOneNeighbor() {
    const int num_swaps = rand_.Uniform(num_swaps_ - 1) + 2;
    std::unordered_set<int> inserted;
    for (int i = 0; i < num_swaps; ++i) {
      int index1 = rand_.Uniform(Size());
      while (ContainsKey(inserted, index1)) {
        index1 = rand_.Uniform(Size());
      }
      inserted.insert(index1);
      int index2 = rand_.Uniform(Size());
      while (ContainsKey(inserted, index2)) {
        index2 = rand_.Uniform(Size());
      }
      inserted.insert(index2);
      SetValue(index1, OldValue(index2));
      SetValue(index2, OldValue(index1));
    }
    if (++round_ > num_rounds_) {
      return false;
    }
    return true;
  }

 private:
  virtual void OnStart() {
    round_ = 0;
  }

  const int num_swaps_;
  const int num_rounds_;
  ACMRandom rand_;
  int round_;
};

class Insert : public IntVarLocalSearchOperator {
 public:
  explicit Insert(const std::vector<IntVar*>& variables, int num_items)
      : IntVarLocalSearchOperator(variables),
        num_items_(num_items),
        index1_(0),
        index2_(0) {}

  virtual ~Insert() {}

 protected:
  // Make a neighbor assigning one variable to its target value.
  virtual bool MakeOneNeighbor() {
    if (!Increment()) {
      return false;
    }
    const int64 value = OldValue(index1_);
    if (index1_ < index2_) {
      // Push down
      for (int i = index1_; i < index2_; ++i) {
        SetValue(i, OldValue(i + 1));
      }
    } else {
      for (int i = index1_; i > index2_; --i) {
        SetValue(i, OldValue(i - 1));
      }
    }
    SetValue(index2_, value);
    return true;
  }

 private:
  bool Increment() {
    const int size = Size();
    index2_++;
    if (index2_ == index1_) {
      index2_++;
    }
    if (index2_ >= size) {
      index2_ = 0;
      index1_++;
    }
    if (index1_ == size - 1) {
      return false;
    }
    return true;
  }

  virtual void OnStart() {
    index1_ = 0;
    index2_ = 0;
  }

  const int num_items_;
  int index1_;
  int index2_;
};

class SmartInsert : public IntVarLocalSearchOperator {
 public:
  explicit SmartInsert(const std::vector<IntVar*>& variables, int num_items)
      : IntVarLocalSearchOperator(variables),
        num_items_(num_items),
        index1_(0),
        index2_(0),
        up_(true) {}

  virtual ~SmartInsert() {}

 protected:
  // Make a neighbor assigning one variable to its target value.
  virtual bool MakeOneNeighbor() {
    if (!Increment()) {
      return false;
    }
    const int64 value = OldValue(index1_);
    if (up_) {
      // find next hole up from index1_.
      int hole = index1_;
      while (hole < Size() && IsProduct(OldValue(hole))) {
        hole++;
      }
      if (hole == Size()) {
        return true;  // Invalid move.
      }
      const int hole_value = OldValue(hole);
      for (int i = hole; i > index1_ + 1; --i) {
        SetValue(i, OldValue(i - 1));
      }
      SetValue(index1_, OldValue(index2_));
      SetValue(index2_, hole_value);
    } else {
      // find next hole down from index1_.
      int hole = index1_;
      while (hole >= 0 && IsProduct(OldValue(hole))) {
        hole--;
      }
      if (hole == -1 || (index2_ >= hole && index2_ <= index1_)) {
        return true;  // Invalid move.
      }
      // Push down
      const int hole_value = OldValue(hole);
      for (int i = hole; i < index1_; ++i) {
        SetValue(i, OldValue(i + 1));
      }
      SetValue(index1_, OldValue(index2_));
      SetValue(index2_, hole_value);
    }
    return true;
  }

 private:
  bool IsProduct(int value) {
    return value >= 0 && value < num_items_;
  }

  bool Increment() {
    if (!up_) {
      up_ = true;
      return true;
    }
    const int size = Size();
    index2_++;
    if (index2_ == index1_) {
      index2_++;
    }
    if (index2_ >= size) {
      index2_ = 0;
      index1_++;
    }
    if (index1_ == size - 1) {
      return false;
    }
    up_ = false;
    return true;
  }

  virtual void OnStart() {
    index1_ = 0;
    index2_ = 0;
    up_ = true;
  }

  const int num_items_;
  int index1_;
  int index2_;
  bool up_;
};

class Filter : public IntVarLocalSearchFilter {
 public:
  explicit Filter(const std::vector<IntVar*>& vars,
                  const std::vector<int>& item_to_product,
                  const std::vector<int>& due_dates,
                  const std::vector<std::vector<int>>& transitions,
                  int inventory_cost)
      : IntVarLocalSearchFilter(vars),
        item_to_product_(item_to_product),
        due_dates_(due_dates),
        transitions_(transitions),
        inventory_cost_(inventory_cost),
        tmp_solution_(vars.size()),
        current_cost_(0) {}

  virtual ~Filter() {}

  virtual void OnSynchronize(const Assignment* delta) {
    for (int i = 0; i < Size(); ++i) {
      const int value = Value(i);
      tmp_solution_[i] = value;
    }
    current_cost_ = Evaluate();
  }

  virtual bool Accept(const Assignment* delta,
                      const Assignment* unused_deltadelta) {
    const Assignment::IntContainer& solution_delta = delta->IntVarContainer();
    const int solution_delta_size = solution_delta.Size();

    // Check for LNS.
    for (int i = 0; i < solution_delta_size; ++i) {
      if (!solution_delta.Element(i).Activated()) {
        return true;
      }
    }

    // Compute delta.
    for (int index = 0; index < solution_delta_size; ++index) {
      int64 touched_var = -1;
      FindIndex(solution_delta.Element(index).Var(), &touched_var);
      const int64 value = solution_delta.Element(index).Value();
      if (value < item_to_product_.size() && touched_var > due_dates_[value]) {
        Backtrack();
        return false;
      }
      if (!FLAGS_use_tabu && !FLAGS_use_sa) {  // We do not evaluate the cost.
        SetTmpSolution(touched_var, value);
      }
    }
    const int new_cost = FLAGS_use_tabu || FLAGS_use_sa ? -1 : Evaluate();
    Backtrack();
    return new_cost < current_cost_;
  }

  void SetTmpSolution(int index, int value) {
    touched_tmp_solution_.push_back(index);
    tmp_solution_[index] = value;
  }

  void Backtrack() {
    for (const int i : touched_tmp_solution_) {
      tmp_solution_[i] = Value(i);
    }
    touched_tmp_solution_.clear();
  }

  int ItemToProduct(int item) const {
    return item >= item_to_product_.size() ? -1 : item_to_product_[item];
  }

  int Evaluate() {
    // Compute costs.
    int previous = -1;
    int transition_cost = 0;
    int earliness_cost = 0;

    for (int i = 0; i < tmp_solution_.size(); ++i) {
      const int item = tmp_solution_[i];
      const int product = ItemToProduct(item);
      if (previous != -1  && product != -1 && previous != product) {
        transition_cost += transitions_[previous][product];
      }
      if (product != -1) {
        previous = product;
        earliness_cost += due_dates_[item] - i;
      }
    }
    return earliness_cost * inventory_cost_ + transition_cost;
  }

 private:
  const std::vector<int> item_to_product_;
  const std::vector<int> due_dates_;
  const std::vector<std::vector<int>> transitions_;
  const int inventory_cost_;
  std::vector<int> tmp_solution_;
  std::vector<int> touched_tmp_solution_;
  int current_cost_;
};

void LoadSolution(const std::string& filename, std::vector<int>* vec) {
  File* const file = File::OpenOrDie(filename, "r");
  std::string line;
  file->ReadToString(&line, 10000);
  const std::vector<std::string> words =
      strings::Split(line, " ", strings::SkipEmpty());
  LOG(INFO) << "Solution file has " << words.size() << " entries";
  vec->clear();
  for (const std::string& word : words) {
    vec->push_back(atoi32(word));
  }
  LOG(INFO) << "  - loaded " << strings::Join(*vec, " ");
}

void Solve(const std::string& filename, const std::string& solution_file) {
  LOG(INFO) << "Load " << filename;
  AcpData data;
  data.Load(filename);

  std::vector<int> solution;
  if (!solution_file.empty()) {
    LoadSolution(solution_file, &solution);
  }

  LOG(INFO) << "  - " << data.num_periods() << " periods";
  LOG(INFO) << "  - " << data.num_products() << " products";
  LOG(INFO) << "  - earliness cost is " << data.inventory_cost();

  int num_items = 0;
  for (const std::vector<int>& d : data.due_dates_per_product()) {
    num_items += d.size();
  }

  LOG(INFO) << "  - " << num_items << " items";
  const int num_residuals = data.num_periods() - num_items;
  LOG(INFO) << "  - " << num_residuals << " non active periods";

  std::vector<int> item_to_product;
  for (int i = 0; i < data.num_products(); ++i) {
    for (int j = 0; j < data.due_dates_per_product()[i].size(); ++j) {
      item_to_product.push_back(i);
    }
  }

  LOG(INFO) << "Build model";
  int max_cost = 0;
  IntTupleSet transition_cost_tuples(5);
  std::vector<int> tuple(5);
  for (int i = 0; i < data.num_products(); ++i) {
    for (int j = 0; j < data.num_products(); ++j) {
      const int cost = data.transitions()[i][j];
      max_cost = std::max(max_cost, cost);
      tuple[0] = i;  // value on timetable
      tuple[1] = i;  // state (last value defined if value is -1)
      tuple[2] = j;  // value on next date
      tuple[3] = j;  // state on next date
      tuple[4] = cost;
      transition_cost_tuples.Insert(tuple);
      tuple[0] = -1;  // Continuation from i, -1 ,j
      transition_cost_tuples.Insert(tuple);
    }
    // Build tuples value, -1 that keeps the state.
    tuple[0] = i;
    tuple[1] = i;
    tuple[2] = -1;
    tuple[3] = i;
    tuple[4] = 0;
    transition_cost_tuples.Insert(tuple);
    tuple[0] = -1;
    tuple[1] = i;
    tuple[2] = -1;
    tuple[3] = i;
    tuple[4] = 0;
    transition_cost_tuples.Insert(tuple);
    // Add transition from initial state.
    tuple[0] = -1;
    tuple[1] = -1;
    tuple[2] = i;
    tuple[3] = i;
    tuple[4] = 0;
    transition_cost_tuples.Insert(tuple);

  }
  // Add initial state in case no production periods are packed at the start.
  tuple[0] = -1;
  tuple[1] = -1;
  tuple[2] = -1;
  tuple[3] = -1;
  tuple[4] = 0;
  transition_cost_tuples.Insert(tuple);
  LOG(INFO) << "  - transition cost tuple set has "
            << transition_cost_tuples.NumTuples() << " tuples";

  IntTupleSet product_tuples(2);
  for (int i = 0; i < item_to_product.size(); ++i) {
    product_tuples.Insert2(i, item_to_product[i]);
  }
  for (int i = 0; i <= num_residuals; ++i) {
    product_tuples.Insert2(num_items + i, -1);
  }
  LOG(INFO) << "  - item to product tuple set has "
            << product_tuples.NumTuples() << " tuples";

  Solver solver("acp_challenge");
  std::vector<IntVar*> products;
  solver.MakeIntVarArray(data.num_periods(), -1, data.num_products() - 1,
                         "product_", &products);
  std::vector<IntVar*> items;
  solver.MakeIntVarArray(data.num_periods(), 0, data.num_periods() - 1,
                         "item_", &items);
  std::vector<IntVar*> deliveries;
  std::vector<int> due_dates;
  LOG(INFO) << "  - build inventory costs";
  std::vector<IntVar*> inventory_costs;
  for (int i = 0; i < data.num_products(); ++i) {
    for (int j = 0; j < data.due_dates_per_product()[i].size(); ++j) {
      const int due_date = data.due_dates_per_product()[i][j];
      IntVar* const delivery =
          solver.MakeIntVar(0, due_date, StringPrintf("delivery_%d_%d", i, j));
      // if (j > 0) {  // Order deliveries of the same product.
      //   solver.AddConstraint(solver.MakeLess(deliveries.back(), delivery));
      // }
      inventory_costs.push_back(
          solver.MakeDifference(due_date, delivery)->Var());
      deliveries.push_back(delivery);
      due_dates.push_back(due_date);
    }
  }
  for (int i = 0; i < num_residuals; ++i) {
    IntVar* const inactive =
        solver.MakeIntVar(0, data.num_periods() - 1, "inactive");
    deliveries.push_back(inactive);
  }
  solver.AddConstraint(
      solver.MakeInversePermutationConstraint(items, deliveries));

  // Link items and tuples.
  for (int p = 0; p < data.num_periods(); ++p) {
    std::vector<IntVar*> tmp(2);
    tmp[0] = items[p];
    tmp[1] = products[p];
    solver.AddConstraint(solver.MakeAllowedAssignments(tmp, product_tuples));
  }

  LOG(INFO) << "  - build transition cost";
  // Create transition costs.
  std::vector<IntVar*> transition_costs;
  solver.MakeIntVarArray(data.num_periods() - 1, 0, max_cost, "transition_cost",
                         &transition_costs);
  std::vector<IntVar*> states;
  solver.MakeIntVarArray(data.num_periods(), -1, data.num_products() - 1,
                         "state_", &states);
  for (int p = 0; p < data.num_periods() - 1; ++p) {
    std::vector<IntVar*> tmp(5);
    tmp[0] = products[p];
    tmp[1] = states[p];
    tmp[2] = products[p + 1];
    tmp[3] = states[p + 1];
    tmp[4] = transition_costs[p];
    solver.AddConstraint(
        solver.MakeAllowedAssignments(tmp, transition_cost_tuples));
  }
  // Special rule for the first element.
  solver.AddConstraint(
      solver.MakeGreaterOrEqual(solver.MakeIsEqualCstVar(states[0], -1),
                                solver.MakeIsEqualCstVar(products[0], -1)));

  // Redundant due date constraints on non variables.
  std::unordered_set<int> due_date_set(due_dates.begin(), due_dates.end());
  for (const int due_date : due_date_set) {
    std::vector<IntVar*> outside;
    int inside_count = 0;
    for (int i = 0; i < due_dates.size(); ++i) {
      const int local_due_date = due_dates[i];
      if (local_due_date <= due_date) {
        inside_count++;
      } else {
        outside.push_back(
            solver.MakeIsLessOrEqualCstVar(deliveries[i], due_date));
      }
    }
    for (int i = due_dates.size(); i < deliveries.size(); ++i) {
      outside.push_back(
          solver.MakeIsLessOrEqualCstVar(deliveries[i], due_date));
    }
    const int slack = due_date - inside_count + 1;
    CHECK_EQ(inside_count + outside.size(), data.num_periods());
    Constraint* const ct = solver.MakeSumLessOrEqual(outside, slack);
    solver.AddConstraint(ct);
  }

  // Create objective.
  IntVar* const objective_var =
      solver.MakeSum(solver.MakeProd(solver.MakeSum(inventory_costs),
                                     data.inventory_cost()),
                     solver.MakeSum(transition_costs))->Var();
  SearchMonitor* const  objective = FLAGS_use_tabu ?
      solver.MakeTabuSearch(false, objective_var, 1, items, FLAGS_tabu_size,
                            FLAGS_tabu_size, FLAGS_tabu_factor) :
      (FLAGS_use_sa ?
       solver.MakeSimulatedAnnealing(false, objective_var, 1,
                                     FLAGS_sa_temperature) :
       solver.MakeMinimize(objective_var, 1));
  // Create search monitors.
  SearchMonitor* const log = solver.MakeSearchLog(1000000, objective_var);

  DecisionBuilder* const db = solver.MakePhase(items,
                                               Solver::CHOOSE_MIN_SIZE,
                                               Solver::ASSIGN_MIN_VALUE);

  DecisionBuilder* const random_db =
      solver.MakePhase(items,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_RANDOM_VALUE);
  SearchLimit* const lns_limit = solver.MakeFailuresLimit(FLAGS_lns_limit);
  DecisionBuilder* const inner_db = solver.MakeSolveOnce(random_db, lns_limit);

  LocalSearchOperator* swap = solver.RevAlloc(new Swap(items));
  LocalSearchOperator* reverse = solver.RevAlloc(new Reverse(items));
  LocalSearchOperator* insert = solver.RevAlloc(new Insert(items, num_items));
  LocalSearchOperator* smart_insert =
      solver.RevAlloc(new SmartInsert(items, num_items));
  LocalSearchOperator* random_swap =
      solver.RevAlloc(new NRandomSwaps(
          items, FLAGS_ls_swaps, FLAGS_ls_rounds, FLAGS_ls_seed));
  LocalSearchOperator* random_lns =
      solver.RevAlloc(new RandomIntervalLns(items,
                                            item_to_product,
                                            FLAGS_lns_size,
                                            FLAGS_lns_intervals,
                                            FLAGS_lns_seed,
                                            FLAGS_lns_product));
  std::vector<LocalSearchOperator*> operators;
  operators.push_back(swap);
  operators.push_back(reverse);
  operators.push_back(smart_insert);
  operators.push_back(insert);
  operators.push_back(random_swap);
  if (FLAGS_use_lns && !FLAGS_use_tabu && !FLAGS_use_sa) {
    operators.push_back(random_lns);
  }

  LocalSearchOperator* const moves =
      solver.ConcatenateOperators(operators, true);

  std::vector<LocalSearchFilter*> filters;
  if (FLAGS_use_filter) {
    filters.push_back(solver.RevAlloc(new Filter(items, item_to_product,
                                                 due_dates, data.transitions(),
                                                 data.inventory_cost())));
  }

  LocalSearchPhaseParameters* const ls_params =
      solver.MakeLocalSearchPhaseParameters(moves, inner_db, nullptr, filters);


  DecisionBuilder* ls_db = nullptr;
  if (solution.empty()) {
    ls_db =  solver.MakeLocalSearchPhase(items, db, ls_params);
  } else {
    vector<int> offsets(data.num_products() + 1, 0);
    for (int i = 0; i < data.num_products(); ++i) {
      offsets[i + 1] = offsets[i] + data.due_dates_per_product()[i].size();
    }
    Assignment* const solution_assignment = solver.MakeAssignment();
    for (int i = 0; i < items.size(); ++i) {
      IntVar* const item = items[i];
      const int value = solution[i] == -1 ? data.num_products() : solution[i];
      const int item_value = offsets[value]++;
      solution_assignment->Add(item);
      solution_assignment->SetRange(item, value, item_value);
    }
    ls_db =  solver.MakeLocalSearchPhase(solution_assignment, ls_params);
  }

  solver.NewSearch(ls_db, objective, log);
  while (solver.NextSolution()) {
    // for (int p = 0; p < data.num_periods(); ++p) {
    //   LOG(INFO) << StringPrintf("%d: %d %d - %s", p, items[p]->Value(),
    //                             products[p]->Value(),
    //                             states[p]->DebugString().c_str());
    // }

    // for (int i = 0; i < num_items; ++i) {
    //   LOG(INFO) << "item = " << i << ", product = " << item_to_product[i]
    //             << ", due date = " << due_dates[i] << ", delivery = "
    //             << deliveries[i]->Value() << ", cost = "
    //             << inventory_costs[i]->Value();
    // }

    std::string result;
    for (int p = 0; p < data.num_periods(); ++p) {
      result += StringPrintf("%" GG_LL_FORMAT "d ", products[p]->Value());
    }
    LOG(INFO) << result;
  }

  solver.EndSearch();
}
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\nThis program runs the ACP 2014 summer school "
    "competition";

int main(int argc, char** argv) {
  FLAGS_log_prefix = false;
  gflags::SetUsageMessage(kUsage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_input.empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }
  operations_research::Solve(FLAGS_input, FLAGS_solution);
}
