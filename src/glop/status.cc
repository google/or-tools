#include "glop/status.h"

#include "base/logging.h"

namespace operations_research {
namespace glop {

Status::Status() : error_code_(NO_ERROR), error_message_() {}

Status::Status(ErrorCode error_code, std::string error_message)
    : error_code_(error_code),
      error_message_(error_code == NO_ERROR ? "" : error_message) {}

const Status Status::OK;

std::string GetErrorCodeString(Status::ErrorCode error_code) {
  switch (error_code) {
    case Status::NO_ERROR:
      return "NO_ERROR";
    case Status::ERROR_LU:
      return "ERROR_LU";
    case Status::ERROR_BOUND:
      return "ERROR_BOUND";
    case Status::ERROR_NULL:
      return "ERROR_NULL";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid Status::ErrorCode " << error_code;
  return "UNKNOWN Status::ErrorCode";
}

}  // namespace glop
}  // namespace operations_research
