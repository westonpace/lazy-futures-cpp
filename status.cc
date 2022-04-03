#include "status.h"

#include <cstdlib>
#include <iostream>
#include <sstream>

namespace futures {

namespace util {
namespace detail {

StringStreamWrapper::StringStreamWrapper()
    : sstream_(std::make_unique<std::ostringstream>()), ostream_(*sstream_) {}

StringStreamWrapper::~StringStreamWrapper() {}

std::string StringStreamWrapper::str() { return sstream_->str(); }

}  // namespace detail
}  // namespace util

Status::Status(StatusCode code, const std::string& msg)
    : Status::Status(code, msg, nullptr) {}

Status::Status(StatusCode code, std::string msg, std::shared_ptr<StatusDetail> detail) {
  state_ = new State;
  state_->code = code;
  state_->msg = std::move(msg);
  if (detail != nullptr) {
    state_->detail = std::move(detail);
  }
}

void Status::CopyFrom(const Status& s) {
  delete state_;
  if (s.state_ == nullptr) {
    state_ = nullptr;
  } else {
    state_ = new State(*s.state_);
  }
}

std::string Status::CodeAsString() const {
  if (state_ == nullptr) {
    return "OK";
  }
  return CodeAsString(code());
}

std::string Status::CodeAsString(StatusCode code) {
  const char* type;
  switch (code) {
  case StatusCode::OK:
    type = "OK";
    break;
  case StatusCode::OutOfMemory:
    type = "Out of memory";
    break;
  case StatusCode::KeyError:
    type = "Key error";
    break;
  case StatusCode::TypeError:
    type = "Type error";
    break;
  case StatusCode::Invalid:
    type = "Invalid";
    break;
  case StatusCode::Cancelled:
    type = "Cancelled";
    break;
  case StatusCode::IOError:
    type = "IOError";
    break;
  case StatusCode::CapacityError:
    type = "Capacity error";
    break;
  case StatusCode::IndexError:
    type = "Index error";
    break;
  case StatusCode::UnknownError:
    type = "Unknown error";
    break;
  case StatusCode::NotImplemented:
    type = "NotImplemented";
    break;
  case StatusCode::SerializationError:
    type = "Serialization error";
    break;
  case StatusCode::CodeGenError:
    type = "CodeGenError in Gandiva";
    break;
  case StatusCode::ExpressionValidationError:
    type = "ExpressionValidationError";
    break;
  case StatusCode::ExecutionError:
    type = "ExecutionError in Gandiva";
    break;
  default:
    type = "Unknown";
    break;
  }
  return std::string(type);
}

std::string Status::ToString() const {
  std::string result(CodeAsString());
  if (state_ == nullptr) {
    return result;
  }
  result += ": ";
  result += state_->msg;
  if (state_->detail != nullptr) {
    result += ". Detail: ";
    result += state_->detail->ToString();
  }

  return result;
}

void Status::Abort() const { Abort(std::string()); }

void Status::Abort(const std::string& message) const {
  std::cerr << "-- Arrow Fatal Error --\n";
  if (!message.empty()) {
    std::cerr << message << "\n";
  }
  std::cerr << ToString() << std::endl;
  std::abort();
}

#ifdef ARROW_EXTRA_ERROR_CONTEXT
void Status::AddContextLine(const char* filename, int line, const char* expr) {
  ARROW_CHECK(!ok()) << "Cannot add context line to ok status";
  std::stringstream ss;
  ss << "\n" << filename << ":" << line << "  " << expr;
  state_->msg += ss.str();
}
#endif

}  // namespace futures
