//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

#pragma once

#include <iostream>
#include <string>
#include <system_error>

#include "mtconnect/config.hpp"

namespace mtconnect::source {
  /// @brief Reasons why the source failed
  enum class ErrorCode
  {
    OK = 0,
    ADAPTER_FAILED,          ///< The adapter failed
    STREAM_CLOSED,           ///< The stream closed
    INSTANCE_ID_CHANGED,     ///< The source instance id changed
    RESTART_STREAM,          ///< The stream needed to be restarted
    RETRY_REQUEST,           ///< The request needs to be retried
    MULTIPART_STREAM_FAILED  ///< The multipart stream failed
  };
}  // namespace mtconnect::source

namespace std {
  template <>
  struct is_error_code_enum<mtconnect::source::ErrorCode> : true_type
  {};

  template <>
  struct is_error_condition_enum<mtconnect::source::ErrorCode> : true_type
  {};
}  // namespace std

namespace mtconnect::source {
  /// @brief Error categories for error reporting using std:error_code and std::error_condition
  struct ErrorCategory : std::error_category
  {
    const char *name() const noexcept override { return "MTConnect::Error"; }
    std::string message(int ec) const override
    {
      switch (static_cast<ErrorCode>(ec))
      {
        case ErrorCode::OK:
          return "No error";

        case ErrorCode::ADAPTER_FAILED:
          return "Adapter failed and cannot recover";

        case ErrorCode::INSTANCE_ID_CHANGED:
          return "The instance Id of an agent has changed";

        case ErrorCode::STREAM_CLOSED:
          return "The stream closed";

        case ErrorCode::RESTART_STREAM:
          return "The data stream needs to restart";

        case ErrorCode::RETRY_REQUEST:
          return "Retry last failed request";

        case ErrorCode::MULTIPART_STREAM_FAILED:
          return "Multipart/x-mixed-replace is not available";

        default:
          return "Unknown mtconnect error";
      }
    }
  };

  AGENT_SYMBOL_VISIBLE inline const std::error_category &TheErrorCategory()
  {
    static const ErrorCategory theErrorCategory {};
    return theErrorCategory;
  }

  inline std::error_code make_error_code(ErrorCode ec)
  {
    return {static_cast<int>(ec), TheErrorCategory()};
  }

  inline std::error_condition make_error_condition(ErrorCode ec)
  {
    return {static_cast<int>(ec), TheErrorCategory()};
  }

}  // namespace mtconnect::source
