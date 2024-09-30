// Copyright 2010-2024 Google LLC
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

// The bool_or_sample_sat command is simple example of the BoolOr constraint.
package main

import (
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
)

func boolOrSampleSat() {
	model := cpmodel.NewCpModelBuilder()

	x := model.NewBoolVar()
	y := model.NewBoolVar()

	model.AddBoolOr(x, y.Not())
}

func main() {
	boolOrSampleSat()
}
