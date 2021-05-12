//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: Advanced server, flex (plain + SSL)
//
//------------------------------------------------------------------------------

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include "configuration/config_options.hpp"
#include "session.hpp"
#include "utilities.hpp"

namespace mtconnect
{
  namespace http_server
  {
    class TlsDector : public std::enable_shared_from_this<TlsDector>
    {
    public:
      TlsDector(boost::asio::ip::tcp::socket &&socket,
                boost::asio::ssl::context &context,
                bool tlsOnly,
                bool allowPuts,
                const std::set<boost::asio::ip::address> &allowPutsFrom,
                const FieldList &list, Dispatch dispatch,
                ErrorFunction error)
      : m_stream(std::move(socket)), m_tlsContext(context), m_tlsOnly(tlsOnly),
        m_allowPuts(allowPuts), m_allowPutsFrom(allowPutsFrom),
        m_fields(list), m_dispatch(dispatch), m_errorFunction(error)
      {
      }
      
      ~TlsDector()
      {        
      }

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
      boost::asio::ssl::context& m_tlsContext;
      boost::beast::flat_buffer m_buffer;
      
      bool m_tlsOnly;
      bool m_allowPuts;
      std::set<boost::asio::ip::address> m_allowPutsFrom;
            
      FieldList m_fields;
      Dispatch m_dispatch;
      ErrorFunction m_errorFunction;
    };
  }
}
