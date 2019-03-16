
namespace mtconnect {
  namespace test {
    using json = nlohmann::json;

    constexpr nlohmann::json::size_type operator "" _S(unsigned long long v)
    {
      return static_cast<nlohmann::json::size_type>(v);
    }
    
    static inline json find(json &array, const char *path, const char *value)
    {
      CPPUNIT_ASSERT(array.is_array());
      nlohmann::json::json_pointer pointer(path);
      json res;
      for (auto &item : array) {
        if (item.at(pointer).get<std::string>() == value) {
          res = item;
          break;
        }
      }
      return res;
    }
  }
}
