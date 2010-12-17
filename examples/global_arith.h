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

class GlobalArithmeticConstraint : public Constraint {
 public:
  GlobalArithmeticConstraint(Solver* const solver);
  virtual ~GlobalArithmeticConstraint();

  virtual void Post();
  virtual void InitialPropagate();
  void Update(int var_index);

  int MakeVarEqualVarPlusOffset(IntVar* const left_var,
                                IntVar* const right_var,
                                int64 right_offset);
  int MakeScalProdGreaterOrEqualConstant(const vector<IntVar*> vars,
                                         const vector<int64> coefficients,
                                         int64 constant);
  int MakeScalProdLessOrEqualConstant(const vector<IntVar*> vars,
                                      const vector<int64> coefficients,
                                      int64 constant);
  int MakeScalProdEqualConstant(const vector<IntVar*> vars,
                                const vector<int64> coefficients,
                                int64 constant);
  int MakeSumGreaterOrEqualConstant(const vector<IntVar*> vars, int64 constant);
  int MakeSumLessOrEqualConstant(const vector<IntVar*> vars, int64 constant);
  int MakeSumEqualConstant(const vector<IntVar*> vars, int64 constant);
  int MakeRowConstraint(int64 lb,
                        const vector<IntVar*> vars,
                        const vector<int64> coefficients,
                        int64 ub);
  int MakeRowConstraint(int64 lb,
                        IntVar* const v1,
                        int64 coeff1,
                        int64 ub);
  int MakeRowConstraint(int64 lb,
                        IntVar* const v1,
                        int64 coeff1,
                        IntVar* const v2,
                        int64 coeff2,
                        int64 ub);
  int MakeRowConstraint(int64 lb,
                        IntVar* const v1,
                        int64 coeff1,
                        IntVar* const v2,
                        int64 coeff2,
                        IntVar* const v3,
                        int64 coeff3,
                        int64 ub);
  int MakeRowConstraint(int64 lb,
                        IntVar* const v1,
                        int64 coeff1,
                        IntVar* const v2,
                        int64 coeff2,
                        IntVar* const v3,
                        int64 coeff3,
                        IntVar* const v4,
                        int64 coeff4,
                        int64 ub);
  int MakeOrConstraint(int left_constraint_index, int right_constraint_index);

  void Add(int constraint_index);
 private:
  int VarIndex(IntVar* const var);
  int Store(ArithmeticConstraint* const constraint);

  scoped_ptr<ArithmeticPropagator> propagator_;
  hash_map<IntVar*, int> var_indices_;
  vector<ArithmeticConstraint*> constraints_;
};

}  // namespace operations_research
#endif  // EXAMPLES_GLOBAL_ARITH_H_
