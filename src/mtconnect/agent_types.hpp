//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mtconnect::device_model::data_item {
  class DataItem;
}

namespace mtconnect {
  using DataItemPtr = std::shared_ptr<device_model::data_item::DataItem>;
  using WeakDataItemPtr = std::weak_ptr<device_model::data_item::DataItem>;
  using AssetChangeList = std::vector<std::pair<std::string, std::string>>;
}
