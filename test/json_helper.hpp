
constexpr inline nlohmann::json::size_type operator "" _S(unsigned long long v)
{
  return static_cast<nlohmann::json::size_type>(v);
}

inline std::string operator "" _S(const char *v, std::size_t)
{
  return std::string(v);
}


static inline nlohmann::json find(nlohmann::json &array, const char *path, const char *value)
{
  //CPPUNIT_ASSERT(array.is_array());
  nlohmann::json::json_pointer pointer(path);
  nlohmann::json res;
  for (auto &item : array) {
    if (item.at(pointer).get<std::string>() == value) {
      res = item;
      break;
    }
  }
  return res;
}
