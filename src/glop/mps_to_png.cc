
// Utility to dump the fill-in layout of the constraint matrix of a MPS file
// to a PNG file.

#include <stdio.h>
#include <string>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/file.h"
#include "glop/mps_reader.h"
#include "glop/lp_data.h"
#include "glop/png_dump.h"
#include "base/status.h"

DEFINE_string(mps_file, "", "MPS input file.");
DEFINE_string(png_file, "", "PNG output file.");

using operations_research::glop::MPSReader;
using operations_research::glop::LinearProgram;

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags( &argc, &argv, true);
  LinearProgram linear_program;
  MPSReader mps_reader;
  CHECK(!FLAGS_mps_file.empty());
  CHECK(!FLAGS_png_file.empty());
  if (mps_reader.LoadFile(FLAGS_mps_file, &linear_program)) {
    std::string output = DumpConstraintMatrixToPng(linear_program);
    CHECK_OK(file::SetContents(FLAGS_png_file, output, file::Defaults()));
  } else {
    LOG(INFO) << "Parse error for " << FLAGS_mps_file;
  }
  return 0;
}
