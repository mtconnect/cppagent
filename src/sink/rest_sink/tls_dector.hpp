//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "configuration/config_options.hpp"
#include "session.hpp"
#include "utilities.hpp"

namespace mtconnect::sink::rest_sink {
  class TlsDector : public std::enable_shared_from_this<TlsDector>
  {
  public:
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

    void fail(boost::system::error_code ec, const std::string &message)
    {
      NAMED_SCOPE("TlsDector::fail");

      LOG(warning) << "Operation failed: " << message;
      if (ec)
      {
        LOG(warning) << "Closing: " << ec.category().message(ec.value()) << " - " << ec.message();
      }
    }

    void run();
    void detect();
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
