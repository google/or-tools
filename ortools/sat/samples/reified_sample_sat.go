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

// The reified_sample_sat command is a simple example of implication constraints.
package main

import (
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
)

func reifiedSampleSat() {
	model := cpmodel.NewCpModelBuilder()

	x := model.NewBoolVar()
	y := model.NewBoolVar()
	b := model.NewBoolVar()

	// First version using a half-reified bool and.
	model.AddBoolAnd(x, y.Not()).OnlyEnforceIf(b)

	// Second version using implications.
	model.AddImplication(b, x)
	model.AddImplication(b, y.Not())

	// Third version using bool or.
	model.AddBoolOr(b.Not(), x)
	model.AddBoolOr(b.Not(), y.Not())
}

func main() {
	reifiedSampleSat()
}
