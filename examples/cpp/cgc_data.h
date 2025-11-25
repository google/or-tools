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

// Two-Dimensional Constrained Guillotine Cutting
//
// The file contains code to load the problem, in the format detailed below.
//
// Input (on different lines):
//    - number of pieces
//    - length and width for the plane rectangle
//    - for each piece (one line for every piece):
//      - length
//      - width
//      - maximum number of pieces of that type that can be cut
//      - value of the piece
//
// For more details and sample input (and format) see:
//    - http://people.brunel.ac.uk/~mastjjb/jeb/orlib/cgcutinfo.html
//    - //ortools/examples/testdata/cgc contains examples
//        of input files.

#ifndef ORTOOLS_EXAMPLES_CGC_DATA_H_
#define ORTOOLS_EXAMPLES_CGC_DATA_H_

#include <string>
#include <vector>

namespace operations_research {

class ConstrainedGuillotineCuttingData {
 public:
  // Each rectangular piece from the input is represented
  // as an instance of this structure.
  struct Piece {
    int length;
    int width;
    int max_appearances;
    int value;
  };

  ConstrainedGuillotineCuttingData() : root_length_(0), root_width_(0) {}

  bool LoadFromFile(const std::string& input_file);

  // Accessors for problem specification data
  int root_length() const { return root_length_; }
  int root_width() const { return root_width_; }
  const std::vector<Piece>& pieces() const { return pieces_; }

 private:
  // main rectangle size
  int root_length_;
  int root_width_;

  std::vector<Piece> pieces_;
};

}  // namespace operations_research

#endif  // ORTOOLS_EXAMPLES_CGC_DATA_H_
