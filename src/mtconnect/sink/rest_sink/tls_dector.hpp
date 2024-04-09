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

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/utilities.hpp"
#include "session.hpp"

namespace mtconnect::sink::rest_sink {
  /// @brief Helper class to detect when a connection is using Transport Layer Security
  ///
  /// This class checks the protocol and then creates a TLS session or a regular HTTP session.
  class TlsDector : public std::enable_shared_from_this<TlsDector>
  {
  public:
    /// @brief Create a TLS detector to create the asynconously check for a secure connection
    /// @param[in] socket incoming socket connection (takes ownership)
    /// @param[in] context asio context
    /// @param[in] tlsOnly only allow TLS connects, reject otherwise
    /// @param[in] allowPuts allow puts
    /// @param[in] allowPutsFrom allow puts from an address
    /// @param[in] list the header fields
    /// @param[in] dispatch a dispatcher function
    /// @param[in] error an error function
    TlsDector(boost::asio::ip::tcp::socket &&socket, boost::asio::ssl::context &context,
              bool tlsOnly, bool allowPuts, const std::set<boost::asio::ip::address> &allowPutsFrom,
              const FieldList &list, Dispatch dispatch, ErrorFunction error)
      : m_stream(std::move(socket)),
        m_tlsContext(context),
        m_tlsOnly(tlsOnly),
        m_allowPuts(allowPuts),
        m_allowPutsFrom(allowPutsFrom),
        m_fields(list),
        m_dispatch(dispatch),
        m_errorFunction(error)
    {}

    ~TlsDector() {}

    /// @brief Method to call when TLS operation fails
    /// @param[in] ec the erro code
    /// @param[in] message the message
    void fail(boost::system::error_code ec, const std::string &message)
    {
      NAMED_SCOPE("TlsDector::fail");

      LOG(warning) << "Operation failed: " << message;
      if (ec)
      {
        LOG(warning) << "Closing: " << ec.category().message(ec.value()) << " - " << ec.message();
      }
    }

    /// @brief ensure the detection is done in the streams executor
    void run();
    /// @brief asyncronously detect an SSL connection.
    /// @note times out after 30 seconds
    void detect();
    /// @brief the detection async callback
    /// @param[in] ec an error code
    /// @param[in] isTls `true` if this is a TLS connection
    void detected(boost::beast::error_code ec, bool isTls);

  protected:
    boost::beast::tcp_stream m_stream;
    boost::asio::ssl::context &m_tlsContext;
    boost::beast::flat_buffer m_buffer;

    bool m_tlsOnly;
    bool m_allowPuts;
    std::set<boost::asio::ip::address> m_allowPutsFrom;

    FieldList m_fields;
    Dispatch m_dispatch;
    ErrorFunction m_errorFunction;
  };
}  // namespace mtconnect::sink::rest_sink
