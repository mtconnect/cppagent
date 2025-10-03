//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "error.hpp"

#include "mtconnect/entity/factory.hpp"

namespace mtconnect::sink::rest_sink {
  using namespace mtconnect::entity;
  using namespace std;

  const std::string Error::nameForCode(ErrorCode code)
  {
    switch (code)
    {
      case ErrorCode::ASSET_NOT_FOUND:
        return "AssetNotFound";

      case ErrorCode::INTERNAL_ERROR:
        return "InternalError";

      case ErrorCode::INVALID_REQUEST:
        return "InvalidRequest";

      case ErrorCode::INVALID_URI:
        return "InvalidURI";

      case ErrorCode::INVALID_XPATH:
        return "InvalidXPath";

      case ErrorCode::NO_DEVICE:
        return "NoDevice";

      case ErrorCode::OUT_OF_RANGE:
        return "OutOfRange";

      case ErrorCode::QUERY_ERROR:
        return "QueryError";

      case ErrorCode::TOO_MANY:
        return "TooMany";

      case ErrorCode::UNAUTHORIZED:
        return "Unauthorized";

      case ErrorCode::UNSUPPORTED:
        return "Unsupported";

      case ErrorCode::INVALID_PARAMETER_VALUE:
        return "InvalidParameterValue";

      case ErrorCode::INVALID_QUERY_PARAMETER:
        return "InvalidQueryParameter";
    }

    return "InternalError";
  }

  const std::string Error::enumForCode(ErrorCode code)
  {
    switch (code)
    {
      case ErrorCode::ASSET_NOT_FOUND:
        return "ASSET_NOT_FOUND";

      case ErrorCode::INTERNAL_ERROR:
        return "INTERNAL_ERROR";

      case ErrorCode::INVALID_REQUEST:
        return "INVALID_REQUEST";

      case ErrorCode::INVALID_URI:
        return "INVALID_URI";

      case ErrorCode::INVALID_XPATH:
        return "INVALID_XPATH";

      case ErrorCode::NO_DEVICE:
        return "NO_DEVICE";

      case ErrorCode::OUT_OF_RANGE:
        return "OUT_OF_RANGE";

      case ErrorCode::QUERY_ERROR:
        return "QUERY_ERROR";

      case ErrorCode::TOO_MANY:
        return "TOO_MANY";

      case ErrorCode::UNAUTHORIZED:
        return "UNAUTHORIZED";

      case ErrorCode::UNSUPPORTED:
        return "UNSUPPORTED";

      case ErrorCode::INVALID_PARAMETER_VALUE:
        return "INVALID_PARAMETER_VALUE";

      case ErrorCode::INVALID_QUERY_PARAMETER:
        return "INVALID_QUERY_PARAMETER";
    }

    return "InternalError";
  }

}  // namespace mtconnect::sink::rest_sink
