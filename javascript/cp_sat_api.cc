// Minimal C API surface for CP-SAT over WASM.
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include <emscripten/emscripten.h>

#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/model.h"

namespace {

using operations_research::sat::CpModelBuilder;
using operations_research::sat::CpModelProto;
using operations_research::sat::CpSolverResponse;
using operations_research::sat::Domain;
using operations_research::sat::LinearExpr;
using operations_research::sat::Model;
using operations_research::sat::NewSatParameters;
using operations_research::sat::SatParameters;
using operations_research::sat::SolveCpModel;

uint8_t* SerializeResponse(const CpSolverResponse& response, size_t* out_len) {
  if (out_len == nullptr) return nullptr;
  std::string data;
  if (!response.SerializeToString(&data)) {
    *out_len = 0;
    return nullptr;
  }
  auto* buffer =
      static_cast<uint8_t*>(std::malloc(sizeof(uint8_t) * data.size()));
  if (buffer == nullptr) {
    *out_len = 0;
    return nullptr;
  }
  std::memcpy(buffer, data.data(), data.size());
  *out_len = data.size();
  return buffer;
}

CpSolverResponse MakeInvalidResponse(const std::string& message) {
  CpSolverResponse response;
  response.set_status(operations_research::sat::CpSolverStatus::MODEL_INVALID);
  response.set_status_detail(message);
  return response;
}

}  // namespace

extern "C" {

// Solve a CpModelProto and optional SatParameters provided as serialized binary
// blobs. Returns a malloc()'d buffer containing a serialized CpSolverResponse.
// The caller owns the buffer and must free it with free_buffer().
EMSCRIPTEN_KEEPALIVE uint8_t* solve_model(const uint8_t* model_data,
                                          size_t model_len,
                                          const uint8_t* params_data,
                                          size_t params_len, size_t* out_len) {
  if (out_len == nullptr) return nullptr;
  *out_len = 0;

  CpModelProto model_proto;
  if (model_data == nullptr || !model_proto.ParseFromArray(model_data,
                                                           static_cast<int>(model_len))) {
    const auto response = MakeInvalidResponse("Failed to parse CpModelProto.");
    return SerializeResponse(response, out_len);
  }

  SatParameters sat_params;
  if (params_data != nullptr && params_len > 0) {
    sat_params.ParseFromArray(params_data, static_cast<int>(params_len));
  }

  Model model;
  model.Add(NewSatParameters(sat_params));

  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  return SerializeResponse(response, out_len);
}

// Convenience: build and solve a magic square of the given size. Returns a
// serialized CpSolverResponse. Intended for quick smoke tests from JS.
EMSCRIPTEN_KEEPALIVE uint8_t* solve_magic_square(int size, int num_workers,
                                                 size_t* out_len) {
  if (out_len == nullptr) return nullptr;
  *out_len = 0;
  if (size <= 0) size = 1;

  CpModelBuilder builder;
  std::vector<std::vector<operations_research::sat::IntVar>> square(size);
  std::vector<operations_research::sat::IntVar> all_variables;
  const Domain domain(1, size * size);
  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      const auto var = builder.NewIntVar(domain);
      square[i].push_back(var);
      all_variables.push_back(var);
    }
  }

  builder.AddAllDifferent(all_variables);

  const int magic_value = size * (size * size + 1) / 2;

  for (int i = 0; i < size; ++i) {
    LinearExpr sum;
    for (int j = 0; j < size; ++j) {
      sum += square[i][j];
    }
    builder.AddEquality(sum, magic_value);
  }
  for (int j = 0; j < size; ++j) {
    LinearExpr sum;
    for (int i = 0; i < size; ++i) {
      sum += square[i][j];
    }
    builder.AddEquality(sum, magic_value);
  }
  LinearExpr diag1_sum;
  LinearExpr diag2_sum;
  for (int i = 0; i < size; ++i) {
    diag1_sum += square[i][i];
    diag2_sum += square[i][size - 1 - i];
  }
  builder.AddEquality(diag1_sum, magic_value);
  builder.AddEquality(diag2_sum, magic_value);

  Model model;
  SatParameters params;
  if (num_workers > 0) {
    params.set_num_search_workers(num_workers);
  }
  model.Add(NewSatParameters(params));

  const CpSolverResponse response = SolveCpModel(builder.Build(), &model);
  return SerializeResponse(response, out_len);
}

// Free a buffer returned by solve_model() / solve_magic_square().
EMSCRIPTEN_KEEPALIVE void free_buffer(uint8_t* ptr) {
  std::free(ptr);
}

}  // extern "C"
