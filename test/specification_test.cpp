// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "checkpoint.hpp"
#include "data_item.hpp"
#include "device.hpp"
#include "globals.hpp"
#include "json_helper.hpp"
#include "json_printer.hpp"
#include "observation.hpp"
#include "test_globals.hpp"
#include "xml_parser.hpp"
#include "xml_printer.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using namespace std;
using namespace mtconnect;
using json = nlohmann::json;

