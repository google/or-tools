// Copyright 2010-2013 Google
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

#include "glop/png_dump.h"

#include "image/base/rawimage.h"
#include "image/codec/pngencoder.h"
#include "glop/lp_data.h"
#include "glop/lp_types.h"
#include "glop/sparse.h"

namespace operations_research {
namespace glop {

std::string DumpConstraintMatrixToPng(const LinearProgram& linear_program) {
  const ColIndex num_cols = linear_program.num_variables();
  const RowIndex num_rows = linear_program.num_constraints();
  image_base::RawImage image;
  image.Resize(ColToIntIndex(num_cols), RowToIntIndex(num_rows),
               image_base::RawImage::GRAYSCALE);
  for (ColIndex col(0); col < num_cols; ++col) {
    // Initialize background to white.
    for (RowIndex row(0); row < num_rows; ++row) {
      image.SetValue(ColToIntIndex(col), RowToIntIndex(row), 0, 255);
    }

    // Draw non-zero entries in black.
    for (const SparseColumn::Entry e : linear_program.GetSparseColumn(col)) {
      if (e.coefficient() != 0.0) {
        image.SetValue(ColToIntIndex(col), RowToIntIndex(e.row()), 0, 0);
      }
    }
  }

  image_codec::PngEncoder png_encoder;
  std::string output;
  png_encoder.EncodeImage(&image, &output);
  return output;
}

}  // namespace glop
}  // namespace operations_research
