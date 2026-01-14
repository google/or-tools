// Minimal C API surface for CP-SAT over WASM.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>

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
using operations_research::sat::StopSearch;

#ifndef ORTOOLS_WASM_ALLOWED_ORIGINS
#define ORTOOLS_WASM_ALLOWED_ORIGINS ""
#endif

EM_JS(char*, GetHostCString, (), {
  var host = "";
  if (typeof location !== 'undefined' && location.hostname) {
    host = location.hostname;
  }
  var len = lengthBytesUTF8(host) + 1;
  var ptr = _malloc(len);
  stringToUTF8(host, ptr, len);
  return ptr;
});

std::string TrimAscii(std::string_view value) {
  const size_t start = value.find_first_not_of(" \t\r\n");
  if (start == std::string_view::npos) return std::string();
  const size_t end = value.find_last_not_of(" \t\r\n");
  return std::string(value.substr(start, end - start + 1));
}

std::string ToLowerAscii(std::string value) {
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}

std::string NormalizeHost(std::string_view value) {
  std::string trimmed = TrimAscii(value);
  if (trimmed.empty()) return trimmed;

  const size_t scheme_pos = trimmed.find("://");
  if (scheme_pos != std::string::npos) {
    trimmed = trimmed.substr(scheme_pos + 3);
  }

  const size_t path_pos = trimmed.find_first_of("/?#");
  if (path_pos != std::string::npos) {
    trimmed = trimmed.substr(0, path_pos);
  }

  const size_t port_pos = trimmed.find(':');
  if (port_pos != std::string::npos) {
    trimmed = trimmed.substr(0, port_pos);
  }

  return ToLowerAscii(trimmed);
}

std::string FetchHost() {
  char* host_ptr = GetHostCString();
  if (host_ptr == nullptr) return std::string();
  std::string host(host_ptr);
  std::free(host_ptr);
  return ToLowerAscii(host);
}

struct OriginCheckResult {
  bool allowed = true;
  std::string error;
};

const OriginCheckResult& CheckOriginAllowed() {
  static OriginCheckResult result = []() {
    OriginCheckResult res;
    const std::string allowlist = ORTOOLS_WASM_ALLOWED_ORIGINS;
    if (allowlist.empty()) {
      res.allowed = true;
      return res;
    }

    const std::string host = FetchHost();
    if (host.empty()) {
      res.allowed = false;
      res.error = "Host unavailable; refusing to run.";
      return res;
    }

    size_t start = 0;
    while (start < allowlist.size()) {
      const size_t comma = allowlist.find(',', start);
      const size_t end = (comma == std::string::npos) ? allowlist.size() : comma;
      const std::string token = NormalizeHost(
          std::string_view(allowlist).substr(start, end - start));
      if (!token.empty() && token == host) {
        res.allowed = true;
        return res;
      }
      if (comma == std::string::npos) break;
      start = comma + 1;
    }

    res.allowed = false;
    res.error = "Host not allowed: " + host;
    return res;
  }();
  return result;
}

// Track the in-flight model so we can interrupt it from JS.
std::mutex g_active_model_mutex;
Model* g_active_model = nullptr;

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

  const OriginCheckResult& origin_check = CheckOriginAllowed();
  if (!origin_check.allowed) {
    const auto response = MakeInvalidResponse(origin_check.error);
    return SerializeResponse(response, out_len);
  }

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

  {
    std::lock_guard<std::mutex> lock(g_active_model_mutex);
    g_active_model = &model;
  }

  const CpSolverResponse response = SolveCpModel(model_proto, &model);

  {
    std::lock_guard<std::mutex> lock(g_active_model_mutex);
    g_active_model = nullptr;
  }
  return SerializeResponse(response, out_len);
}

// Free a buffer returned by solve_model() / validate_model().
EMSCRIPTEN_KEEPALIVE void free_buffer(uint8_t* ptr) {
  std::free(ptr);
}

// Interrupt the currently running solve, if any.
EMSCRIPTEN_KEEPALIVE void interrupt_solve() {
  std::lock_guard<std::mutex> lock(g_active_model_mutex);
  if (g_active_model != nullptr) {
    StopSearch(g_active_model);
  }
}

// Validate a CpModelProto. Returns nullptr if valid, otherwise a malloc()'d
// buffer containing a UTF-8 error message.
EMSCRIPTEN_KEEPALIVE uint8_t* validate_model(const uint8_t* model_data,
                                             size_t model_len,
                                             size_t* out_len) {
  if (out_len == nullptr) return nullptr;
  *out_len = 0;

  const OriginCheckResult& origin_check = CheckOriginAllowed();
  if (!origin_check.allowed) {
    return CopyStringToBuffer(origin_check.error, out_len);
  }

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
