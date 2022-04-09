
//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <memory>
#include <boost/beast/core/error.hpp>

#include "url_parser.hpp"

namespace mtconnect::adapter::agent {

  class Session : public std::enable_shared_from_this<Session>
  {
  public:
    using Connected = std::function<void(boost::beast::error_code ec)>;
    using Result = std::function<void(boost::beast::error_code ec,
                                      const std::string &result)>;

    virtual ~Session() {}
    virtual void connect(const Url &url, Connected cb) = 0;
    virtual bool isOpen() const  = 0;
    virtual void stop() = 0;
    
    virtual bool makeRequest(const std::string &suffix,
                             const UrlQuery &query,
                             bool stream,
                             Result cb) = 0;
  };

}  // namespace mtconnect::adapter::agent
