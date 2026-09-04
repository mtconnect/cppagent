//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//

#pragma once

#include <optional>
#include <string>

#include "response.hpp"
#include "session.hpp"

namespace mtconnect::sink::rest_sink {
  /// Completes a routed request while consistently propagating the request id.
  inline void respond(SessionPtr session, ResponsePtr&& response,
                      std::optional<std::string> requestId = std::nullopt)
  {
    response->m_requestId = std::move(requestId);
    session->writeResponse(std::move(response));
  }
}  // namespace mtconnect::sink::rest_sink
