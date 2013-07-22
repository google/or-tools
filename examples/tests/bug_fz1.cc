// Copyright 2011-2012 Google
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

#include "base/hash.h"
#include "base/map-util.h"
#include "base/stl_util.h"
#include "base/random.h"
#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/constraint_solver.h"
#include "util/string_array.h"

namespace operations_research {
void ShoppingBasketBug() {
  Solver s("ShoppingBasketBug");
  IntVar* const x15 = s.MakeIntVar(13, 18, "x15");
  IntVar* const x18 = s.MakeIntVar(13, 18, "x18");
  // IsLessEqualCstCt((((Watch<x[15] == 18>(0 .. 1) * 1132) + (Watch<x[18] == 18>(0 .. 1) * 1970)) + 3787), 4499, Is((((Watch<x[15] == 18>(0 .. 1) * 1132) + (Watch<x[18] == 18>(0 .. 1) * 1970)) + 3787) <= 4499)(0 .. 1))
  IntVar* const is_less =
      s.MakeIsLessOrEqualCstVar(
          s.MakeSum(
              s.MakeSum(
                  s.MakeProd(s.MakeIsEqualCstVar(x15, 18), 1132),
                  s.MakeProd(s.MakeIsEqualCstVar(x18, 18), 1970)),
              3787),
          4499);
// IntElementConstraint([1130, 1602, 1354, 99999, 1117, 1132], (x[15](13..18) + -13), Var<IntElement([1130, 1602, 1354, 99999, 1117, 1132], (x[15](13..18) + -13))>(1117 1130 1132 1354 1602 99999))
  std::vector<int64> values1;
  values1.push_back(1130);
  values1.push_back(1602);
  values1.push_back(1354);
  values1.push_back(99999);
  values1.push_back(1117);
  values1.push_back(1132);
  IntVar* const elem1 = s.MakeElement(values1, s.MakeSum(x15, -13)->Var())->Var();
// IntElementConstraint([1908, 99999, 2093, 3060, 2256, 1970], (x[18](13..18) + -13), Var<IntElement([1908, 99999, 2093, 3060, 2256, 1970], (x[18](13..18) + -13))>(1908 1970 2093 2256 3060 99999))
  std::vector<int64> values2;
  values2.push_back(1908);
  values2.push_back(99999);
  values2.push_back(2093);
  values2.push_back(3060);
  values2.push_back(2256);
  values2.push_back(1970);
  IntVar* const elem2 = s.MakeElement(values2, s.MakeSum(x18, -13)->Var())->Var();
// cast(MinIntExpr(BOOL____00117(0 .. 1), BOOL____00118(0 .. 1)), Var<MinIntExpr(BOOL____00117(0 .. 1), BOOL____00118(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____00180(0 .. 1), BOOL____00181(0 .. 1)), Var<MinIntExpr(BOOL____00180(0 .. 1), BOOL____00181(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____00243(0 .. 1), BOOL____00244(0 .. 1)), Var<MinIntExpr(BOOL____00243(0 .. 1), BOOL____00244(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____00369(0 .. 1), BOOL____00370(0 .. 1)), Var<MinIntExpr(BOOL____00369(0 .. 1), BOOL____00370(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____00432(0 .. 1), BOOL____00433(0 .. 1)), Var<MinIntExpr(BOOL____00432(0 .. 1), BOOL____00433(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____00495(0 .. 1), BOOL____00496(0 .. 1)), Var<MinIntExpr(BOOL____00495(0 .. 1), BOOL____00496(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____00621(0 .. 1), BOOL____00622(0 .. 1)), Var<MinIntExpr(BOOL____00621(0 .. 1), BOOL____00622(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____00810(0 .. 1), BOOL____00811(0 .. 1)), Var<MinIntExpr(BOOL____00810(0 .. 1), BOOL____00811(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____00873(0 .. 1), BOOL____00874(0 .. 1)), Var<MinIntExpr(BOOL____00873(0 .. 1), BOOL____00874(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____01062(0 .. 1), BOOL____01063(0 .. 1)), Var<MinIntExpr(BOOL____01062(0 .. 1), BOOL____01063(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____01125(0 .. 1), BOOL____01126(0 .. 1)), Var<MinIntExpr(BOOL____01125(0 .. 1), BOOL____01126(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____01251(0 .. 1), BOOL____01252(0 .. 1)), Var<MinIntExpr(BOOL____01251(0 .. 1), BOOL____01252(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____01314(0 .. 1), BOOL____01315(0 .. 1)), Var<MinIntExpr(BOOL____01314(0 .. 1), BOOL____01315(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____01377(0 .. 1), BOOL____01378(0 .. 1)), Var<MinIntExpr(BOOL____01377(0 .. 1), BOOL____01378(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____01440(0 .. 1), BOOL____01441(0 .. 1)), Var<MinIntExpr(BOOL____01440(0 .. 1), BOOL____01441(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____01503(0 .. 1), BOOL____01504(0 .. 1)), Var<MinIntExpr(BOOL____01503(0 .. 1), BOOL____01504(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____01566(0 .. 1), BOOL____01567(0 .. 1)), Var<MinIntExpr(BOOL____01566(0 .. 1), BOOL____01567(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____01629(0 .. 1), BOOL____01630(0 .. 1)), Var<MinIntExpr(BOOL____01629(0 .. 1), BOOL____01630(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____01692(0 .. 1), BOOL____01693(0 .. 1)), Var<MinIntExpr(BOOL____01692(0 .. 1), BOOL____01693(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____01755(0 .. 1), BOOL____01756(0 .. 1)), Var<MinIntExpr(BOOL____01755(0 .. 1), BOOL____01756(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____01818(0 .. 1), BOOL____01819(0 .. 1)), Var<MinIntExpr(BOOL____01818(0 .. 1), BOOL____01819(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____01881(0 .. 1), BOOL____01882(0 .. 1)), Var<MinIntExpr(BOOL____01881(0 .. 1), BOOL____01882(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____02007(0 .. 1), BOOL____02008(0 .. 1)), Var<MinIntExpr(BOOL____02007(0 .. 1), BOOL____02008(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____02070(0 .. 1), BOOL____02071(0 .. 1)), Var<MinIntExpr(BOOL____02070(0 .. 1), BOOL____02071(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____02133(0 .. 1), BOOL____02134(0 .. 1)), Var<MinIntExpr(BOOL____02133(0 .. 1), BOOL____02134(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____02259(0 .. 1), BOOL____02260(0 .. 1)), Var<MinIntExpr(BOOL____02259(0 .. 1), BOOL____02260(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____02322(0 .. 1), BOOL____02323(0 .. 1)), Var<MinIntExpr(BOOL____02322(0 .. 1), BOOL____02323(0 .. 1))>(0 .. 1))
// cast(MinIntExpr(BOOL____02385(0 .. 1), BOOL____02386(0 .. 1)), Var<MinIntExpr(BOOL____02385(0 .. 1), BOOL____02386(0 .. 1))>(0 .. 1))
  std::vector<IntVar*> ands;
  s.MakeBoolVarArray(28, "ands", &ands);
// Sum(Var<IntElement([1130, 1602, 1354, 99999, 1117, 1132], (x[15](13..18) + -13))>(1117 1130 1132 1354 1602 99999), (Var<MinIntExpr(BOOL____00495(0 .. 1), BOOL____00496(0 .. 1))>(0 .. 1) * 398), (Var<MinIntExpr(BOOL____00243(0 .. 1), BOOL____00244(0 .. 1))>(0 .. 1) * 395), (Var<MinIntExpr(BOOL____00432(0 .. 1), BOOL____00433(0 .. 1))>(0 .. 1) * 450), (Var<MinIntExpr(BOOL____02322(0 .. 1), BOOL____02323(0 .. 1))>(0 .. 1) * 390), (Var<MinIntExpr(BOOL____01125(0 .. 1), BOOL____01126(0 .. 1))>(0 .. 1) * 450), (Var<MinIntExpr(BOOL____00180(0 .. 1), BOOL____00181(0 .. 1))>(0 .. 1) * 490), (Var<MinIntExpr(BOOL____02259(0 .. 1), BOOL____02260(0 .. 1))>(0 .. 1) * 395), (Var<MinIntExpr(BOOL____02133(0 .. 1), BOOL____02134(0 .. 1))>(0 .. 1) * 395), (Var<MinIntExpr(BOOL____02070(0 .. 1), BOOL____02071(0 .. 1))>(0 .. 1) * 295), (Var<MinIntExpr(BOOL____02007(0 .. 1), BOOL____02008(0 .. 1))>(0 .. 1) * 395), (Var<MinIntExpr(BOOL____01881(0 .. 1), BOOL____01882(0 .. 1))>(0 .. 1) * 345), (Var<MinIntExpr(BOOL____01818(0 .. 1), BOOL____01819(0 .. 1))>(0 .. 1) * 395), (Var<MinIntExpr(BOOL____01755(0 .. 1), BOOL____01756(0 .. 1))>(0 .. 1) * 350), (Var<MinIntExpr(BOOL____01692(0 .. 1), BOOL____01693(0 .. 1))>(0 .. 1) * 390), (Var<MinIntExpr(BOOL____01629(0 .. 1), BOOL____01630(0 .. 1))>(0 .. 1) * 450), (Var<MinIntExpr(BOOL____01566(0 .. 1), BOOL____01567(0 .. 1))>(0 .. 1) * 390), (Var<MinIntExpr(BOOL____01503(0 .. 1), BOOL____01504(0 .. 1))>(0 .. 1) * 395), (Var<MinIntExpr(BOOL____01440(0 .. 1), BOOL____01441(0 .. 1))>(0 .. 1) * 395), (Var<MinIntExpr(BOOL____01377(0 .. 1), BOOL____01378(0 .. 1))>(0 .. 1) * 390), (Var<MinIntExpr(BOOL____01314(0 .. 1), BOOL____01315(0 .. 1))>(0 .. 1) * 395), (Var<MinIntExpr(BOOL____00873(0 .. 1), BOOL____00874(0 .. 1))>(0 .. 1) * 395), (Var<MinIntExpr(BOOL____00810(0 .. 1), BOOL____00811(0 .. 1))>(0 .. 1) * 450), (Var<MinIntExpr(BOOL____00621(0 .. 1), BOOL____00622(0 .. 1))>(0 .. 1) * 300), (Var<MinIntExpr(BOOL____00369(0 .. 1), BOOL____00370(0 .. 1))>(0 .. 1) * 400), (Var<MinIntExpr(BOOL____01251(0 .. 1), BOOL____01252(0 .. 1))>(0 .. 1) * 390), (Is((((Watch<x[15] == 18>(0 .. 1) * 1132) + (Watch<x[18] == 18>(0 .. 1) * 1970)) + 3787) <= 4499)(0 .. 1) * 290), Var<IntElement([1908, 99999, 2093, 3060, 2256, 1970], (x[18](13..18) + -13))>(1908 1970 2093 2256 3060 99999), (Var<MinIntExpr(BOOL____00117(0 .. 1), BOOL____00118(0 .. 1))>(0 .. 1) * 395), (Var<MinIntExpr(BOOL____02385(0 .. 1), BOOL____02386(0 .. 1))>(0 .. 1) * 399), (Var<MinIntExpr(BOOL____01062(0 .. 1), BOOL____01063(0 .. 1))>(0 .. 1) * 390)) == ScalProd([Var<IntElement([1130, 1602, 1354, 99999, 1117, 1132], (x[15](13..18) + -13))>, Var<MinIntExpr(BOOL____00495(0 .. 1), BOOL____00496(0 .. 1))>, Var<MinIntExpr(BOOL____00243(0 .. 1), BOOL____00244(0 .. 1))>, Var<MinIntExpr(BOOL____00432(0 .. 1), BOOL____00433(0 .. 1))>, Var<MinIntExpr(BOOL____02322(0 .. 1), BOOL____02323(0 .. 1))>, Var<MinIntExpr(BOOL____01125(0 .. 1), BOOL____01126(0 .. 1))>, Var<MinIntExpr(BOOL____00180(0 .. 1), BOOL____00181(0 .. 1))>, Var<MinIntExpr(BOOL____02259(0 .. 1), BOOL____02260(0 .. 1))>, Var<MinIntExpr(BOOL____02133(0 .. 1), BOOL____02134(0 .. 1))>, Var<MinIntExpr(BOOL____02070(0 .. 1), BOOL____02071(0 .. 1))>, Var<MinIntExpr(BOOL____02007(0 .. 1), BOOL____02008(0 .. 1))>, Var<MinIntExpr(BOOL____01881(0 .. 1), BOOL____01882(0 .. 1))>, Var<MinIntExpr(BOOL____01818(0 .. 1), BOOL____01819(0 .. 1))>, Var<MinIntExpr(BOOL____01755(0 .. 1), BOOL____01756(0 .. 1))>, Var<MinIntExpr(BOOL____01692(0 .. 1), BOOL____01693(0 .. 1))>, Var<MinIntExpr(BOOL____01629(0 .. 1), BOOL____01630(0 .. 1))>, Var<MinIntExpr(BOOL____01566(0 .. 1), BOOL____01567(0 .. 1))>, Var<MinIntExpr(BOOL____01503(0 .. 1), BOOL____01504(0 .. 1))>, Var<MinIntExpr(BOOL____01440(0 .. 1), BOOL____01441(0 .. 1))>, Var<MinIntExpr(BOOL____01377(0 .. 1), BOOL____01378(0 .. 1))>, Var<MinIntExpr(BOOL____01314(0 .. 1), BOOL____01315(0 .. 1))>, Var<MinIntExpr(BOOL____00873(0 .. 1), BOOL____00874(0 .. 1))>, Var<MinIntExpr(BOOL____00810(0 .. 1), BOOL____00811(0 .. 1))>, Var<MinIntExpr(BOOL____00621(0 .. 1), BOOL____00622(0 .. 1))>, Var<MinIntExpr(BOOL____00369(0 .. 1), BOOL____00370(0 .. 1))>, Var<MinIntExpr(BOOL____01251(0 .. 1), BOOL____01252(0 .. 1))>, Is((((Watch<x[15] == 18>(0 .. 1) * 1132) + (Watch<x[18] == 18>(0 .. 1) * 1970)) + 3787) <= 4499), Var<IntElement([1908, 99999, 2093, 3060, 2256, 1970], (x[18](13..18) + -13))>, Var<MinIntExpr(BOOL____00117(0 .. 1), BOOL____00118(0 .. 1))>, Var<MinIntExpr(BOOL____02385(0 .. 1), BOOL____02386(0 .. 1))>, Var<MinIntExpr(BOOL____01062(0 .. 1), BOOL____01063(0 .. 1))>], [1, 398, 395, 450, 390, 450, 490, 395, 395, 295, 395, 345, 395, 350, 390, 450, 390, 395, 395, 390, 395, 395, 450, 300, 400, 390, 290, 1, 395, 399, 390])(3025..211355)
  std::vector<IntVar*> vars;
  vars.push_back(elem1);
  vars.push_back(ands[0]);
  vars.push_back(ands[1]);
  vars.push_back(ands[2]);
  vars.push_back(ands[3]);
  vars.push_back(ands[4]);
  vars.push_back(ands[5]);
  vars.push_back(ands[6]);
  vars.push_back(ands[7]);
  vars.push_back(ands[8]);
  vars.push_back(ands[9]);
  vars.push_back(ands[10]);
  vars.push_back(ands[11]);
  vars.push_back(ands[12]);
  vars.push_back(ands[13]);
  vars.push_back(ands[14]);
  vars.push_back(ands[15]);
  vars.push_back(ands[16]);
  vars.push_back(ands[17]);
  vars.push_back(ands[18]);
  vars.push_back(ands[19]);
  vars.push_back(ands[20]);
  vars.push_back(ands[21]);
  vars.push_back(ands[22]);
  vars.push_back(ands[23]);
  vars.push_back(ands[24]);
  vars.push_back(is_less);
  vars.push_back(elem2);
  vars.push_back(ands[25]);
  vars.push_back(ands[26]);
  vars.push_back(ands[27]);

  // Var<IntElement([1130, 1602, 1354, 99999, 1117, 1132], (x[15](13..18) + -13))>,
  // Var<MinIntExpr(BOOL____00495(0 .. 1), BOOL____00496(0 .. 1))>,
  // Var<MinIntExpr(BOOL____00243(0 .. 1), BOOL____00244(0 .. 1))>,
  // Var<MinIntExpr(BOOL____00432(0 .. 1), BOOL____00433(0 .. 1))>,
  // Var<MinIntExpr(BOOL____02322(0 .. 1), BOOL____02323(0 .. 1))>,
  // Var<MinIntExpr(BOOL____01125(0 .. 1), BOOL____01126(0 .. 1))>,
  // Var<MinIntExpr(BOOL____00180(0 .. 1), BOOL____00181(0 .. 1))>,
  // Var<MinIntExpr(BOOL____02259(0 .. 1), BOOL____02260(0 .. 1))>,
  // Var<MinIntExpr(BOOL____02133(0 .. 1), BOOL____02134(0 .. 1))>,
  // Var<MinIntExpr(BOOL____02070(0 .. 1), BOOL____02071(0 .. 1))>,

  // Var<MinIntExpr(BOOL____02007(0 .. 1), BOOL____02008(0 .. 1))>,
  // Var<MinIntExpr(BOOL____01881(0 .. 1), BOOL____01882(0 .. 1))>,
  // Var<MinIntExpr(BOOL____01818(0 .. 1), BOOL____01819(0 .. 1))>,
  // Var<MinIntExpr(BOOL____01755(0 .. 1), BOOL____01756(0 .. 1))>,
  // Var<MinIntExpr(BOOL____01692(0 .. 1), BOOL____01693(0 .. 1))>,
  // Var<MinIntExpr(BOOL____01629(0 .. 1), BOOL____01630(0 .. 1))>,
  // Var<MinIntExpr(BOOL____01566(0 .. 1), BOOL____01567(0 .. 1))>,
  // Var<MinIntExpr(BOOL____01503(0 .. 1), BOOL____01504(0 .. 1))>,
  // Var<MinIntExpr(BOOL____01440(0 .. 1), BOOL____01441(0 .. 1))>,
  // Var<MinIntExpr(BOOL____01377(0 .. 1), BOOL____01378(0 .. 1))>,

  // Var<MinIntExpr(BOOL____01314(0 .. 1), BOOL____01315(0 .. 1))>,
  // Var<MinIntExpr(BOOL____00873(0 .. 1), BOOL____00874(0 .. 1))>,
  // Var<MinIntExpr(BOOL____00810(0 .. 1), BOOL____00811(0 .. 1))>,
  // Var<MinIntExpr(BOOL____00621(0 .. 1), BOOL____00622(0 .. 1))>,
  // Var<MinIntExpr(BOOL____00369(0 .. 1), BOOL____00370(0 .. 1))>,
  // Var<MinIntExpr(BOOL____01251(0 .. 1), BOOL____01252(0 .. 1))>,
  // Is((((Watch<x[15] == 18>(0 .. 1) * 1132) + (Watch<x[18] == 18>(0 .. 1) * 1970)) + 3787) <= 4499),
  // Var<IntElement([1908, 99999, 2093, 3060, 2256, 1970], (x[18](13..18) + -13))>,
  // Var<MinIntExpr(BOOL____00117(0 .. 1), BOOL____00118(0 .. 1))>,
  // Var<MinIntExpr(BOOL____02385(0 .. 1), BOOL____02386(0 .. 1))>,

  // Var<MinIntExpr(BOOL____01062(0 .. 1), BOOL____01063(0 .. 1))>
  std::vector<int64> coefs;
  coefs.push_back(1);
  coefs.push_back(398);
  coefs.push_back(395);
  coefs.push_back(450);
  coefs.push_back(390);
  coefs.push_back(450);
  coefs.push_back(490);
  coefs.push_back(395);
  coefs.push_back(395);
  coefs.push_back(295);

  coefs.push_back(395);
  coefs.push_back(345);
  coefs.push_back(395);
  coefs.push_back(350);
  coefs.push_back(390);
  coefs.push_back(450);
  coefs.push_back(390);
  coefs.push_back(395);
  coefs.push_back(395);
  coefs.push_back(390);

  coefs.push_back(395);
  coefs.push_back(395);
  coefs.push_back(450);
  coefs.push_back(300);
  coefs.push_back(400);
  coefs.push_back(390);
  coefs.push_back(290);
  coefs.push_back(1);
  coefs.push_back(395);
  coefs.push_back(399);

  coefs.push_back(390);
  IntVar* const obj = s.MakeScalProd(vars, coefs)->Var();


}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  return 0;
}
