// Minimal C API surface for CP-SAT over WASM.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include <emscripten/bind.h>
#include <emscripten/emscripten.h>

#include "generated_proto_schemas.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/model.h"

namespace {

using operations_research::sat::CpModelProto;
using operations_research::sat::CpSolverResponse;
using operations_research::sat::Model;
using operations_research::sat::NewSatParameters;
using operations_research::sat::SatParameters;
using operations_research::sat::SolveCpModel;
using operations_research::sat::wasm::kCpModelProtoSchema;
using operations_research::sat::wasm::kSatParametersProtoSchema;

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
  response.set_solution_info(message);
  return response;
}

uint8_t* CopyStringToBuffer(const std::string& message, size_t* out_len) {
  if (out_len == nullptr) return nullptr;
  *out_len = message.size();
  if (message.empty()) return nullptr;
  auto* buffer =
      static_cast<uint8_t*>(std::malloc(sizeof(uint8_t) * message.size()));
  if (buffer == nullptr) {
    *out_len = 0;
    return nullptr;
  }
  std::memcpy(buffer, message.data(), message.size());
  return buffer;
}

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE const char* get_cp_model_schema() {
  return kCpModelProtoSchema;
}

EMSCRIPTEN_KEEPALIVE const char* get_sat_parameters_schema() {
  return kSatParametersProtoSchema;
}

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

// Free a buffer returned by solve_model() / validate_model().
EMSCRIPTEN_KEEPALIVE void free_buffer(uint8_t* ptr) {
  std::free(ptr);
}

// Validate a CpModelProto. Returns nullptr if valid, otherwise a malloc()'d
// buffer containing a UTF-8 error message.
EMSCRIPTEN_KEEPALIVE uint8_t* validate_model(const uint8_t* model_data,
                                             size_t model_len,
                                             size_t* out_len) {
  if (out_len == nullptr) return nullptr;
  *out_len = 0;

  CpModelProto model_proto;
  if (model_data == nullptr ||
      !model_proto.ParseFromArray(model_data, static_cast<int>(model_len))) {
    return CopyStringToBuffer("Failed to parse CpModelProto.", out_len);
  }

  const std::string validation =
      operations_research::sat::ValidateCpModel(model_proto);
  if (validation.empty()) return nullptr;
  return CopyStringToBuffer(validation, out_len);
}

}  // extern "C"

namespace {

std::string GetCpModelSchemaString() { return kCpModelProtoSchema; }
std::string GetSatParametersSchemaString() { return kSatParametersProtoSchema; }

}  // namespace

EMSCRIPTEN_BINDINGS(cp_sat_api_bindings) {
  emscripten::function("getCpModelSchema", &GetCpModelSchemaString);
  emscripten::function("getSatParametersSchema", &GetSatParametersSchemaString);
}
