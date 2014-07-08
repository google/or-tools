
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
