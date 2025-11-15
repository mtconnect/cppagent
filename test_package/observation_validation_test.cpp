//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology (“AMT”)
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

/// @file
/// Observation validation tests

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include <chrono>

#include "mtconnect/asset/asset.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/json_mapper.hpp"
#include "mtconnect/pipeline/shdr_token_mapper.hpp"
#include "mtconnect/pipeline/timestamp_extractor.hpp"
#include "mtconnect/pipeline/validator.hpp"

using namespace mtconnect;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace mtconnect::asset;
using namespace mtconnect::device_model::data_item;
using namespace std;
using namespace date::literals;
using namespace std::literals;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class MockPipelineContract : public PipelineContract
{
public:
  MockPipelineContract(int32_t schemaVersion, DataItemPtr &dataItem)
    : m_schemaVersion(schemaVersion), m_dataItem(dataItem)
  {}
  DevicePtr findDevice(const std::string &) override { return m_device; }
  DataItemPtr findDataItem(const std::string &device, const std::string &name) override
  {
    return m_dataItem;
  }
  void eachDataItem(EachDataItem fun) override {}
  void deliverObservation(observation::ObservationPtr obs) override {}
  void deliverAsset(AssetPtr) override {}
  void deliverDevices(std::list<DevicePtr>) override {}
  void deliverDevice(DevicePtr device) override {}
  int32_t getSchemaVersion() const override { return m_schemaVersion; }
  bool isValidating() const override { return m_validation; }
  void deliverAssetCommand(entity::EntityPtr) override {}
  void deliverCommand(entity::EntityPtr) override {}
  void deliverConnectStatus(entity::EntityPtr, const StringList &, bool) override {}
  void sourceFailed(const std::string &id) override {}
  const ObservationPtr checkDuplicate(const ObservationPtr &obs) const override { return obs; }

  int32_t m_schemaVersion;
  DataItemPtr &m_dataItem;
  DevicePtr m_device;
  bool m_validation {true};
};

/// @brief Validation tests for observations
class ObservationValidationTest : public testing::Test
{
protected:
  void SetUp() override
  {
    ErrorList errors;
    m_dataItem =
        DataItem::make({{"id", "exec"s}, {"category", "EVENT"s}, {"type", "EXECUTION"s}}, errors);

    m_context = make_shared<PipelineContext>();
    m_context->m_contract = make_unique<MockPipelineContract>(SCHEMA_VERSION(2, 5), m_dataItem);
    m_validator = make_shared<Validator>(m_context);
    m_validator->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
    m_time = Timestamp(date::sys_days(2021_y / jan / 19_d)) + 10h + 1min;
  }

  void TearDown() override
  {
    m_validator.reset();
    m_dataItem.reset();
    m_context.reset();
  }

  shared_ptr<Validator> m_validator;
  shared_ptr<PipelineContext> m_context;
  DataItemPtr m_dataItem;
  Timestamp m_time;
};

/// @test Validate a valid value for Execution
TEST_F(ObservationValidationTest, should_validate_value)
{
  ErrorList errors;
  auto event = Observation::make(m_dataItem, {{"VALUE", "READY"s}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);
}

/// @test Unavailable should always be valid
TEST_F(ObservationValidationTest, unavailable_should_be_valid)
{
  ErrorList errors;
  auto event = Observation::make(m_dataItem, {{"VALUE", "UNAVAILABLE"s}}, m_time, errors);
  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);
}

/// @test Invalid values should be marked as invalid
TEST_F(ObservationValidationTest, should_detect_invalid_value)
{
  ErrorList errors;
  auto event = Observation::make(m_dataItem, {{"VALUE", "FLABOR"s}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("INVALID", quality);
}

/// @test Unknown types should be unverifiable
TEST_F(ObservationValidationTest, should_not_validate_unknown_type)
{
  ErrorList errors;
  m_dataItem =
      DataItem::make({{"id", "exec"s}, {"category", "EVENT"s}, {"type", "x:FLABOR"s}}, errors);

  auto event = Observation::make(m_dataItem, {{"VALUE", "FLABOR"s}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("UNVERIFIABLE", quality);
}

/// @test Tag deprecated values
TEST_F(ObservationValidationTest, should_set_deprecated_flag_when_deprecated)
{
  ErrorList errors;
  m_dataItem =
      DataItem::make({{"id", "exec"s}, {"category", "EVENT"s}, {"type", "EXECUTION"s}}, errors);

  auto event = Observation::make(m_dataItem, {{"VALUE", "PROGRAM_OPTIONAL_STOP"s}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);

  auto dep = evt->get<bool>("deprecated");
  ASSERT_TRUE(dep);
}

/// @test Only deprecate when the version is earlier than the current version
TEST_F(ObservationValidationTest, should_not_set_deprecated_flag_when_deprecated_version_greater)
{
  ErrorList errors;
  m_dataItem =
      DataItem::make({{"id", "exec"s}, {"category", "EVENT"s}, {"type", "EXECUTION"s}}, errors);

  auto contract = static_cast<MockPipelineContract *>(m_context->m_contract.get());
  contract->m_schemaVersion = SCHEMA_VERSION(1, 3);

  auto event = Observation::make(m_dataItem, {{"VALUE", "PROGRAM_OPTIONAL_STOP"s}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);

  ASSERT_FALSE(evt->hasProperty("deprecated"));
}

/// @test do not validate data sets
TEST_F(ObservationValidationTest, should_not_validate_data_sets)
{
  ErrorList errors;
  m_dataItem = DataItem::make({{"id", "exec"s},
                               {"category", "EVENT"s},
                               {"type", "EXECUTION"s},
                               {"representation", "DATA_SET"s}},
                              errors);
  ASSERT_TRUE(m_dataItem->isDataSet());

  auto event =
      Observation::make(m_dataItem, {{"VALUE", DataSet({{"field", "value"s}})}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);
}

/// @test do not validate tables
TEST_F(ObservationValidationTest, should_not_validate_tables)
{
  ErrorList errors;
  m_dataItem = DataItem::make({{"id", "exec"s},
                               {"category", "EVENT"s},
                               {"type", "EXECUTION"s},
                               {"representation", "TABLE"s}},
                              errors);
  ASSERT_TRUE(m_dataItem->isDataSet());

  auto event =
      Observation::make(m_dataItem, {{"VALUE", DataSet({{"field", "value"s}})}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);
}

TEST_F(ObservationValidationTest, should_be_invalid_if_entry_has_not_been_introduced_yet)
{
  ErrorList errors;
  m_dataItem =
      DataItem::make({{"id", "exec"s}, {"category", "EVENT"s}, {"type", "EXECUTION"s}}, errors);

  auto contract = static_cast<MockPipelineContract *>(m_context->m_contract.get());
  contract->m_schemaVersion = SCHEMA_VERSION(1, 4);

  auto event = Observation::make(m_dataItem, {{"VALUE", "WAIT"s}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("INVALID", quality);
  ASSERT_FALSE(evt->hasProperty("deprecated"));
}

TEST_F(ObservationValidationTest, should_validate_invalid_sample_value)
{
  auto contract = static_cast<MockPipelineContract *>(m_context->m_contract.get());
  contract->m_schemaVersion = SCHEMA_VERSION(2, 5);

  shared_ptr<ShdrTokenMapper> mapper;
  mapper = make_shared<ShdrTokenMapper>(m_context, "", 2);
  mapper->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));

  ErrorList errors;
  m_dataItem = DataItem::make(
      {{"id", "pos"s}, {"category", "SAMPLE"s}, {"type", "POSITION"s}, {"units", "MILLIMETER"s}},
      errors);

  auto ts = make_shared<Timestamped>();
  ts->m_tokens = {{"pos"s, "ABC"s}};
  ts->m_timestamp = chrono::system_clock::now();
  ts->setProperty("timestamp", ts->m_timestamp);

  auto observations = (*mapper)(ts);
  auto &r = *observations;
  ASSERT_EQ(typeid(Observations), typeid(r));

  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto oi = oblist.begin();

  {
    auto sample = dynamic_pointer_cast<Sample>(*oi++);
    ASSERT_TRUE(sample);
    ASSERT_EQ(m_dataItem, sample->getDataItem());
    ASSERT_TRUE(sample->isUnavailable());

    ASSERT_EQ("INVALID", sample->get<string>("quality"));
  }
}

TEST_F(ObservationValidationTest, should_validate_sample)
{
  auto contract = static_cast<MockPipelineContract *>(m_context->m_contract.get());
  contract->m_schemaVersion = SCHEMA_VERSION(2, 5);

  shared_ptr<ShdrTokenMapper> mapper;
  mapper = make_shared<ShdrTokenMapper>(m_context, "", 2);
  mapper->bind(m_validator);

  ErrorList errors;
  m_dataItem = DataItem::make(
      {{"id", "pos"s}, {"category", "SAMPLE"s}, {"type", "POSITION"s}, {"units", "MILLIMETER"s}},
      errors);

  auto ts = make_shared<Timestamped>();
  ts->m_tokens = {{"pos"s, "1.234"s}};
  ts->m_timestamp = chrono::system_clock::now();
  ts->setProperty("timestamp", ts->m_timestamp);

  auto observations = (*mapper)(ts);
  auto &r = *observations;
  ASSERT_EQ(typeid(Observations), typeid(r));

  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto oi = oblist.begin();

  {
    auto sample = dynamic_pointer_cast<Sample>(*oi++);
    ASSERT_TRUE(sample);
    ASSERT_EQ(m_dataItem, sample->getDataItem());
    ASSERT_FALSE(sample->isUnavailable());

    ASSERT_EQ("VALID", sample->get<string>("quality"));
  }
}

TEST_F(ObservationValidationTest, should_validate_sample_with_int64_value)
{
  ErrorList errors;
  m_dataItem = DataItem::make(
      {{"id", "pos"s}, {"category", "SAMPLE"s}, {"type", "POSITION"s}, {"units", "MILLIMETER"s}},
      errors);

  auto obs = Observation::make(m_dataItem, {{"VALUE", int64_t(100)}}, m_time, errors);

  auto evt = (*m_validator)(std::move(obs));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);
}

TEST_F(ObservationValidationTest, should_not_validate_if_validation_is_off)
{
  auto contract = static_cast<MockPipelineContract *>(m_context->m_contract.get());
  contract->m_schemaVersion = SCHEMA_VERSION(2, 5);
  contract->m_validation = false;

  shared_ptr<ShdrTokenMapper> mapper;
  mapper = make_shared<ShdrTokenMapper>(m_context, "", 2);
  mapper->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));

  ErrorList errors;
  m_dataItem = DataItem::make(
      {{"id", "pos"s}, {"category", "SAMPLE"s}, {"type", "POSITION"s}, {"units", "MILLIMETER"s}},
      errors);

  auto ts = make_shared<Timestamped>();
  ts->m_tokens = {{"pos"s, "ABC"s}};
  ts->m_timestamp = chrono::system_clock::now();
  ts->setProperty("timestamp", ts->m_timestamp);

  auto observations = (*mapper)(ts);
  auto &r = *observations;
  ASSERT_EQ(typeid(Observations), typeid(r));

  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto oi = oblist.begin();

  {
    auto sample = dynamic_pointer_cast<Sample>(*oi++);
    ASSERT_TRUE(sample);
    ASSERT_EQ(m_dataItem, sample->getDataItem());
    ASSERT_TRUE(sample->isUnavailable());

    ASSERT_FALSE(sample->hasProperty("quality"));
  }
}

TEST_F(ObservationValidationTest, should_validate_json_data_item_types)
{
  using namespace mtconnect::device_model;

  ErrorList errors;
  auto contract = static_cast<MockPipelineContract *>(m_context->m_contract.get());
  contract->m_schemaVersion = SCHEMA_VERSION(2, 5);
  Properties dev {
      {"id", "3"s}, {"name", "DeviceTest2"s}, {"uuid", "UnivUniqId2"s}, {"iso841Class", "6"s}};
  auto device = dynamic_pointer_cast<Device>(Device::getFactory()->make("Device", dev, errors));

  shared_ptr<JsonMapper> mapper;
  mapper = make_shared<JsonMapper>(m_context);
  mapper->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));

  m_dataItem = DataItem::make(
      {{"id", "pos"s}, {"category", "SAMPLE"s}, {"type", "POSITION"s}, {"units", "MILLIMETER"s}},
      errors);
  contract->m_dataItem = m_dataItem;
  device->addDataItem(m_dataItem, errors);
  ASSERT_EQ(0, errors.size());

  auto jm = make_shared<JsonMessage>();
  jm->setValue(R"(
{
  "timestamp": "2023-11-09T11:20:00Z",
  "pos": "ABC"
}
)"s);
  jm->m_device = device;

  auto observations = (*mapper)(jm);
  auto oblist = observations->getValue<EntityList>();
  ASSERT_EQ(1, oblist.size());

  auto oi = oblist.begin();

  {
    auto sample = dynamic_pointer_cast<Sample>(*oi++);
    ASSERT_TRUE(sample);
    ASSERT_EQ(m_dataItem, sample->getDataItem());
    ASSERT_TRUE(sample->isUnavailable());

    ASSERT_EQ("INVALID", sample->get<string>("quality"));
  }
}

/// @test Validate a sample
TEST_F(ObservationValidationTest, should_validate_sample_double_value)
{
  ErrorList errors;
  auto event = Observation::make(m_dataItem, {{"VALUE", "READY"s}}, m_time, errors);

  auto evt = (*m_validator)(std::move(event));
  auto quality = evt->get<string>("quality");
  ASSERT_EQ("VALID", quality);
}
