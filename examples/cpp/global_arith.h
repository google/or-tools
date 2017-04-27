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

#ifndef EXAMPLES_GLOBAL_ARITH_H_
#define EXAMPLES_GLOBAL_ARITH_H_
namespace operations_research {
class ArithmeticPropagator;
class ArithmeticConstraint;

class ConstraintRef {
 public:
  ConstraintRef(int index) : index_(index) {}
  int index() const { return index_; }
 private:
  const int index_;
};

class GlobalArithmeticConstraint : public Constraint {
 public:
  GlobalArithmeticConstraint(Solver* const solver);
  virtual ~GlobalArithmeticConstraint();

  virtual void Post();
  virtual void InitialPropagate();
  void Update(int var_index);

  ConstraintRef MakeScalProdGreaterOrEqualConstant(
      const vector<IntVar*> vars,
      const vector<int64> coefficients,
      int64 constant);
  ConstraintRef MakeScalProdLessOrEqualConstant(
      const vector<IntVar*> vars,
      const vector<int64> coefficients,
      int64 constant);
  ConstraintRef MakeScalProdEqualConstant(const vector<IntVar*> vars,
                                          const vector<int64> coefficients,
                                          int64 constant);
  ConstraintRef MakeSumGreaterOrEqualConstant(const vector<IntVar*> vars,
                                              int64 constant);
  ConstraintRef MakeSumLessOrEqualConstant(const vector<IntVar*> vars,
                                           int64 constant);
  ConstraintRef MakeSumEqualConstant(const vector<IntVar*> vars,
                                     int64 constant);
  ConstraintRef MakeRowConstraint(int64 lb,
                                  const vector<IntVar*> vars,
                                  const vector<int64> coefficients,
                                  int64 ub);
  ConstraintRef MakeRowConstraint(int64 lb,
                                  IntVar* const v1,
                                  int64 coeff1,
                                  int64 ub);
  ConstraintRef MakeRowConstraint(int64 lb,
                                  IntVar* const v1,
                                  int64 coeff1,
                                  IntVar* const v2,
                                  int64 coeff2,
                                  int64 ub);
  ConstraintRef MakeRowConstraint(int64 lb,
                                  IntVar* const v1,
                                  int64 coeff1,
                                  IntVar* const v2,
                                  int64 coeff2,
                                  IntVar* const v3,
                                  int64 coeff3,
                                  int64 ub);
  ConstraintRef MakeRowConstraint(int64 lb,
                                  IntVar* const v1,
                                  int64 coeff1,
                                  IntVar* const v2,
                                  int64 coeff2,
                                  IntVar* const v3,
                                  int64 coeff3,
                                  IntVar* const v4,
                                  int64 coeff4,
                                  int64 ub);
  ConstraintRef MakeOrConstraint(ConstraintRef left_constraint_index,
                                 ConstraintRef right_constraint_index);

  void Add(ConstraintRef constraint_index);
 private:
  int VarIndex(IntVar* const var);
  ConstraintRef Store(ArithmeticConstraint* const constraint);

  scoped_ptr<ArithmeticPropagator> propagator_;
  std::unordered_map<IntVar*, int> var_indices_;
  vector<ArithmeticConstraint*> constraints_;
};

}  // namespace operations_research
#endif  // EXAMPLES_GLOBAL_ARITH_H_
