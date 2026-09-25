//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <boost/algorithm/string.hpp>
#include <boost/beast/http/verb.hpp>

#include <iostream>
#include <list>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "mtconnect/config.hpp"
#include "mtconnect/logging.hpp"
#include "parameter.hpp"
#include "request.hpp"
#include "session.hpp"

namespace mtconnect::sink::rest_sink {
  class Session;
  using SessionPtr = std::shared_ptr<Session>;

  /// @brief A REST routing that parses a URI pattern and associates a lambda when it is matched
  /// against a request
  class AGENT_LIB_API Routing
  {
  public:
    using Function = std::function<bool(SessionPtr, RequestPtr)>;
    using PathMatcher = std::function<bool(const std::string&)>;

    Routing(const Routing& r) = default;
    /// @brief Create a routing with a string
    ///
    /// Creates a routing with a regular expression from the string to match against the path
    /// @param[in] verb The `GET`, `PUT`, `POST`, and `DELETE` version of the HTTP request
    /// @param[in] pattern the URI pattern to parse and match
    /// @param[in] function the function to call if matches
    /// @param[in] swagger `true` if swagger related
    Routing(boost::beast::http::verb verb, const std::string& pattern, const Function function,
            bool swagger = false, std::optional<std::string> request = std::nullopt)
      : m_verb(verb), m_command(request), m_function(function), m_swagger(swagger)
    {
      std::string s(pattern);

      auto qp = s.find_first_of('?');
      if (qp != std::string::npos)
      {
        auto query = s.substr(qp + 1);
        s.erase(qp);

        queryParameters(query);
      }

      m_path.emplace(s);
      pathParameters(s);
    }

    /// @brief Create a routing with a regular expression
    ///
    /// Creates a routing from the regular expression to match against the path
    /// @param[in] verb The `GET`, `PUT`, `POST`, and `DELETE` version of the HTTP request
    /// @param[in] pattern the URI pattern to parse and match
    /// @param[in] function the function to call if matches
    /// @param[in] swagger `true` if swagger related
    Routing(boost::beast::http::verb verb, const std::regex& pattern, const Function function,
            bool swagger = false, std::optional<std::string> request = std::nullopt)
      : m_verb(verb),
        m_pattern(pattern),
        m_command(request),
        m_function(function),
        m_swagger(swagger),
        m_catchAll(true)
    {}

    /// @brief Create a routing with a path predicate
    ///
    /// Creates a catch-all routing that matches any path for which the predicate returns `true`.
    /// Prefer this to a regular expression for routes that match arbitrary length paths since
    /// some `std::regex` implementations recurse per character and can exhaust the stack.
    /// @param[in] verb The `GET`, `PUT`, `POST`, and `DELETE` version of the HTTP request
    /// @param[in] matcher predicate called with the request path
    /// @param[in] function the function to call if matches
    /// @param[in] swagger `true` if swagger related
    Routing(boost::beast::http::verb verb, const PathMatcher& matcher, const Function function,
            bool swagger = false, std::optional<std::string> request = std::nullopt)
      : m_verb(verb),
        m_matcher(matcher),
        m_command(request),
        m_function(function),
        m_swagger(swagger),
        m_catchAll(true)
    {}

    /// @brief Added summary and description to the routing
    /// @param[in] summary optional summary
    /// @param[in] description optional description of the routing
    Routing& document(std::optional<std::string> summary,
                      std::optional<std::string> description = std::nullopt)
    {
      m_summary = summary;
      m_description = description;
      return *this;
    }

    /// @brief Added summary and description to the routing
    /// @param[in] summary optional summary
    /// @param[in] description optional description of the routing
    Routing& documentParameter(const std::string& name, UrlPart part,
                               std::optional<std::string> description)
    {
      Parameter* param {nullptr};
      if (part == PATH)
      {
        for (auto& p : m_pathParameters)
        {
          if (p.m_name == name)
          {
            param = &p;
            break;
          }
        }
      }
      else
      {
        for (auto& p : m_queryParameters)
        {
          if (p.m_name == name)
          {
            param = const_cast<Parameter*>(&p);
            break;
          }
        }
      }

      if (param != nullptr)
        param->m_description = description;

      return *this;
    }

    /// @brief Document using common parameter documentation
    /// @param[in] docs common documentation for parameters
    Routing& documentParameters(const ParameterDocList& docs)
    {
      for (const auto& doc : docs)
      {
        documentParameter(doc.m_name, doc.m_part, doc.m_description);
      }
      return *this;
    }

    /// @brief Get the description of the REST call for Swagger
    /// @returns optional string if description is givem
    const auto& getDescription() const { return m_description; }
    /// @brief Get the brief summary fo the REST call for Swagger
    /// @returns optional string if summary is givem
    const auto& getSummary() const { return m_summary; }

    /// @brief Get the list of path position in order
    /// @return the parameter list
    const ParameterList& getPathParameters() const { return m_pathParameters; }
    /// @brief get the unordered set of query parameters
    const QuerySet& getQueryParameters() const { return m_queryParameters; }

    /// @brief run the session's request if this routing matches
    ///
    /// Call the associated lambda when matched
    ///
    /// @param[in] session the session making the request to pass to the Routing if matched
    /// @param[in,out] request the incoming request with a verb and a path
    /// @return `true` if the request was matched
    bool run(SessionPtr session, RequestPtr request)
    {
      if (validateRequest(session, request))
        return m_function(session, request);
      else
        return false;
    }

    /// @brief check if the routing matches the request
    ///
    /// @param[in] session the session making the request to pass to the Routing if matched
    /// @param[in,out] request the incoming request with a verb and a path
    /// @return `true` if the request was matched
    /// @throws `RestError` if there are any parameter errors
    bool matches(SessionPtr session, RequestPtr request)
    {
      if (request->m_command)
      {
        return m_command == *request->m_command && m_verb == request->m_verb;
      }
      else
      {
        request->m_parameters.clear();
        std::vector<std::string> values;
        if (m_verb == request->m_verb && matchPath(request->m_path, &values))
        {
          auto s = values.begin();
          for (auto& p : m_pathParameters)
          {
            if (s != values.end())
            {
              ParameterValue v(*s);
              request->m_parameters.emplace(make_pair(p.m_name, v));
              s++;
            }
          }

          entity::EntityList errors;
          for (auto& p : m_queryParameters)
          {
            auto q = request->m_query.find(p.m_name);
            if (q != request->m_query.end())
            {
              try
              {
                auto v = convertValue(q->second, p.m_type);
                request->m_parameters.emplace(make_pair(p.m_name, v));
              }
              catch (ParameterError& e)
              {
                std::string msg = std::string("query parameter '") + p.m_name + "': " + e.what();

                LOG(warning) << "Parameter error: " << msg;
                auto error = InvalidParameterValue::make(p.m_name, q->second, p.getTypeName(),
                                                         p.getTypeFormat(), msg);
                errors.emplace_back(error);
              }
            }
            else if (!std::holds_alternative<std::monostate>(p.m_default))
            {
              request->m_parameters.emplace(make_pair(p.m_name, p.m_default));
            }
          }

          if (!errors.empty())
            throw RestError(errors, request->m_accepts);

          return true;
        }
        else
        {
          return false;
        }
      }
    }

    /// @brief Validate the request parameters without matching the path
    /// @param[in] session the session making the request to pass to the Routing if matched
    /// @param[in,out] request the incoming request with a verb and a path
    /// @return `true` if the request was matched
    /// @throws `RestError` if there are any parameter errors
    bool validateRequest(SessionPtr session, RequestPtr request)
    {
      entity::EntityList errors;
      /// Just validate the types of the parameters
      for (auto& p : m_pathParameters)
      {
        auto it = request->m_parameters.find(p.m_name);
        if (it != request->m_parameters.end())
        {
          if (!validateValueType(p.m_type, it->second))
          {
            std::string msg = std::string("path parameter '") + p.m_name +
                              "': invalid type, expected " + p.getTypeFormat();
            LOG(warning) << "Parameter error: " << msg;
            auto error = InvalidParameterValue::make(p.m_name, Parameter::toString(it->second),
                                                     p.getTypeName(), p.getTypeFormat(), msg);
            errors.emplace_back(error);
          }
        }
      }

      for (auto& p : m_queryParameters)
      {
        auto it = request->m_parameters.find(p.m_name);
        if (it != request->m_parameters.end())
        {
          if (!validateValueType(p.m_type, it->second))
          {
            std::string msg = std::string("query parameter '") + p.m_name +
                              "': invalid type, expected " + p.getTypeFormat();
            LOG(warning) << "Parameter error: " << msg;
            auto error = InvalidParameterValue::make(p.m_name, Parameter::toString(it->second),
                                                     p.getTypeName(), p.getTypeFormat(), msg);
            errors.emplace_back(error);
          }
        }
        else if (!std::holds_alternative<std::monostate>(p.m_default))
        {
          request->m_parameters.emplace(make_pair(p.m_name, p.m_default));
        }
      }

      if (!errors.empty())
        throw RestError(errors, request->m_accepts);

      return true;
    }

    /// @brief check if the routing's path pattern matches a given path (ignoring verb)
    /// @param[in] path the request path to test
    /// @return `true` if the path matches this routing's pattern
    bool matchesPath(const std::string& path) const { return matchPath(path, nullptr); }

    /// @brief check if this is related to a swagger API
    /// @returns `true` if related to swagger
    auto isSwagger() const { return m_swagger; }

    /// @brief Get the path component of the routing pattern
    const auto& getPath() const { return m_path; }
    /// @brief Get the routing `verb`
    const auto& getVerb() const { return m_verb; }

    /// @brief Check if the route is a catch-all (every path segment is a parameter)
    /// @returns `true` if all path segments are parameters (e.g. `/{device}`)
    auto isCatchAll() const { return m_catchAll; }

    /// @brief Get the optional command associated with the routing
    /// @returns optional routing
    const auto& getCommand() const { return m_command; }

    /// @brief Sets the command associated with this routing for use with websockets
    /// @param command the command
    auto& command(const std::string& command)
    {
      m_command = command;
      return *this;
    }

  protected:
    /// @brief match a path against the segments without using `std::regex`
    ///
    /// Equivalent to matching `/seg1/seg2/.../?` where a parameter segment is
    /// `prefix([^/]+)suffix`. Iterative so arbitrarily long paths are safe.
    /// @param[in] path the request path
    /// @param[out] values if not null, the parameter values in order
    /// @returns `true` if the path matches
    bool matchSegments(std::string_view path, std::vector<std::string>* values) const
    {
      if (m_segments.empty())
        return path.empty();

      for (const auto& segment : m_segments)
      {
        if (path.empty() || path.front() != '/')
          return false;
        path.remove_prefix(1);

        auto next = path.find('/');
        auto part = path.substr(0, next);
        path = next == std::string_view::npos ? std::string_view() : path.substr(next);

        if (segment.m_param)
        {
          if (part.size() <= segment.m_prefix.size() + segment.m_suffix.size() ||
              !part.starts_with(segment.m_prefix) || !part.ends_with(segment.m_suffix))
            return false;
          if (values != nullptr)
          {
            part.remove_prefix(segment.m_prefix.size());
            part.remove_suffix(segment.m_suffix.size());
            values->emplace_back(part);
          }
        }
        else if (part != segment.m_prefix)
        {
          return false;
        }
      }

      return path.empty();
    }

    /// @brief check if the path matches this routing
    /// @param[in] path the request path
    /// @param[out] values if not null, the parameter values in order
    /// @returns `true` if the path matches
    bool matchPath(const std::string& path, std::vector<std::string>* values) const
    {
      if (m_matcher)
        return m_matcher(path);
      if (m_segments.empty() && !m_path)
      {
        // Created from a regular expression
        std::smatch m;
        if (!std::regex_match(path, m, m_pattern))
          return false;
        if (values != nullptr)
          for (auto s = std::next(m.begin()); s != m.end(); s++)
            values->emplace_back(s->str());
        return true;
      }

      std::string_view sv(path);
      if (values != nullptr)
        values->clear();
      if (matchSegments(sv, values))
        return true;

      // Allow a single optional trailing slash
      if (!sv.empty() && sv.back() == '/')
      {
        sv.remove_suffix(1);
        if (values != nullptr)
          values->clear();
        return matchSegments(sv, values);
      }

      return false;
    }

    void pathParameters(std::string s)
    {
      using namespace boost::algorithm;
      using SplitList = std::list<boost::iterator_range<std::string::iterator>>;

      SplitList parts;
      auto pos = s.find_first_not_of('/');
      if (pos != std::string::npos)
      {
        auto range = boost::make_iterator_range(s.begin() + pos, s.end());
        split(parts, range, [](char c) { return c == '/'; });
      }

      bool hasLiteral = false;
      for (auto& p : parts)
      {
        auto start = p.begin();
        auto end = p.end();

        auto openBrace = std::find(start, end, '{');
        decltype(openBrace) closeBrace {end};
        if (openBrace != end && std::distance(openBrace, end) > 2)
          closeBrace = std::find(openBrace + 1, end, '}');

        Segment segment;
        if (openBrace != end && closeBrace != end)
        {
          segment.m_param = true;
          segment.m_prefix = std::string(start, openBrace);
          segment.m_suffix = std::string(closeBrace + 1, end);
          if (!segment.m_prefix.empty() || !segment.m_suffix.empty())
            hasLiteral = true;
          std::string_view param(openBrace + 1, closeBrace);
          m_pathParameters.emplace_back(param);
        }
        else
        {
          segment.m_prefix = std::string(start, end);
          hasLiteral = true;
        }
        m_segments.emplace_back(std::move(segment));
      }

      // A route is catch-all if it has parameters but no literal path segments
      m_catchAll = !m_pathParameters.empty() && !hasLiteral;
    }

    void queryParameters(std::string s)
    {
      std::regex reg("([^=]+)=\\{([^}]+)\\}&?");
      std::smatch match;

      while (regex_search(s, match, reg))
      {
        Parameter qp(match[1]);
        qp.m_part = QUERY;

        getTypeAndDefault(match[2], qp);

        m_queryParameters.emplace(qp);
        s = match.suffix().str();
      }
    }

    void getTypeAndDefault(const std::string& type, Parameter& par)
    {
      std::string t(type);
      auto dp = t.find_first_of(':');
      std::string def;
      if (dp != std::string::npos)
      {
        def = t.substr(dp + 1);
        t.erase(dp);
      }

      if (t == "string")
      {
        par.m_type = STRING;
      }
      else if (t == "integer")
      {
        par.m_type = INTEGER;
      }
      else if (t == "unsigned_integer")
      {
        par.m_type = UNSIGNED_INTEGER;
      }
      else if (t == "double")
      {
        par.m_type = DOUBLE;
      }
      else if (t == "bool")
      {
        par.m_type = BOOL;
      }

      if (!def.empty())
      {
        par.m_default = convertValue(def, par.m_type);
      }
    }

    ParameterValue convertValue(const std::string& s, ParameterType t) const
    {
      switch (t)
      {
        case STRING:
          return s;

        case NONE:
          throw ParameterError("Cannot convert to NONE");

        case DOUBLE:
        {
          char* ep = nullptr;
          const char* sp = s.c_str();
          double r = strtod(sp, &ep);
          if (ep == sp)
            throw ParameterError("cannot convert string '" + s + "' to double");
          return r;
        }

        case INTEGER:
        {
          char* ep = nullptr;
          const char* sp = s.c_str();
          int32_t r = int32_t(strtoll(sp, &ep, 10));
          if (ep == sp)
            throw ParameterError("cannot convert string '" + s + "' to integer");

          return r;
        }

        case UNSIGNED_INTEGER:
        {
          char* ep = nullptr;
          const char* sp = s.c_str();
          uint64_t r = strtoull(sp, &ep, 10);
          if (ep == sp)
            throw ParameterError("cannot convert string '" + s + "' to unsigned integer");

          return r;
        }

        case BOOL:
        {
          return bool(s == "true" || s == "yes");
        }
      }

      throw ParameterError("Unknown type for conversion: " + std::to_string(int(t)));

      return ParameterValue();
    }

    bool validateValueType(ParameterType t, ParameterValue& value)
    {
      switch (t)
      {
        case STRING:
          return std::holds_alternative<std::string>(value);

        case NONE:
          return std::holds_alternative<std::monostate>(value);

        case DOUBLE:
          if (std::holds_alternative<int32_t>(value))
            value = double(std::get<int32_t>(value));
          else if (std::holds_alternative<uint64_t>(value))
            value = double(std::get<uint64_t>(value));

          return std::holds_alternative<double>(value);

        case INTEGER:
          if (std::holds_alternative<uint64_t>(value))
          {
            auto v = std::get<uint64_t>(value);
            if (v <= uint64_t(std::numeric_limits<int32_t>::max()))
              value = int32_t(v);
          }
          else if (std::holds_alternative<double>(value))
          {
            auto v = std::get<double>(value);
            if (v >= double(std::numeric_limits<int32_t>::min()) &&
                v <= double(std::numeric_limits<int32_t>::max()))
              value = int32_t(v);
          }
          return std::holds_alternative<int32_t>(value);

        case UNSIGNED_INTEGER:
          if (std::holds_alternative<int32_t>(value))
          {
            auto v = std::get<int32_t>(value);
            if (v >= 0)
              value = uint64_t(v);
          }
          else if (std::holds_alternative<double>(value))
          {
            auto v = std::get<double>(value);
            if (v >= 0 && v <= double(std::numeric_limits<uint64_t>::max()))
              value = uint64_t(v);
          }
          return std::holds_alternative<uint64_t>(value);

        case BOOL:
          return std::holds_alternative<bool>(value);
      }
      return false;
    }

  protected:
    boost::beast::http::verb m_verb;
    struct Segment
    {
      std::string m_prefix;  ///< literal text, or text before the parameter
      std::string m_suffix;  ///< text after the parameter
      bool m_param = false;
    };

    std::regex m_pattern;
    PathMatcher m_matcher;
    std::vector<Segment> m_segments;
    std::optional<std::string> m_path;
    ParameterList m_pathParameters;
    QuerySet m_queryParameters;
    std::optional<std::string> m_command;
    Function m_function;

    std::optional<std::string> m_summary;
    std::optional<std::string> m_description;

    bool m_swagger = false;
    bool m_catchAll = false;
  };
}  // namespace mtconnect::sink::rest_sink
