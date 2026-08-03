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

#pragma once

#include "mtconnect/config.hpp"
#include "mtconnect/observation/observation.hpp"
#include "ruby_entity.hpp"
#include "ruby_vm.hpp"

namespace mtconnect::ruby {
  using namespace mtconnect::observation;
  using namespace std;

  struct RubyObservation
  {
    static RClass *m_eventClass;
    static RClass *m_sampleClass;
    static RClass *m_conditionClass;

    static void initialize(mrb_state *mrb, RClass *module)
    {
      auto entityClass = mrb_class_get_under(mrb, module, "Entity");
      auto observationClass = mrb_define_class_under(mrb, module, "Observation", entityClass);
      MRB_SET_INSTANCE_TT(observationClass, MRB_TT_DATA);

      m_eventClass = mrb_define_class_under(mrb, module, "Event", observationClass);
      MRB_SET_INSTANCE_TT(m_eventClass, MRB_TT_DATA);

      m_sampleClass = mrb_define_class_under(mrb, module, "Sample", observationClass);
      MRB_SET_INSTANCE_TT(m_sampleClass, MRB_TT_DATA);

      m_conditionClass = mrb_define_class_under(mrb, module, "Condition", observationClass);
      MRB_SET_INSTANCE_TT(m_conditionClass, MRB_TT_DATA);

      mrb_define_class_method(
          mrb, observationClass, "make",
          [](mrb_state *mrb, mrb_value self) {
            using namespace device_model::data_item;

            mrb_value di;
            mrb_value props;
            mrb_value ts;

            auto count = mrb_get_args(mrb, "oo|o", &di, &props, &ts);
            auto dataItem = MRubySharedPtr<Entity>::unwrap<DataItem>(mrb, di);

            if (count < 3)
            {
              auto time = std::chrono::system_clock::now();
              ts = toRuby(mrb, time);
            }

            struct RClass *klass;
            switch (dataItem->getCategory())
            {
              case DataItem::SAMPLE:
                klass = m_sampleClass;
                break;

              case DataItem::EVENT:
                klass = m_eventClass;
                break;

              case DataItem::CONDITION:
                klass = m_conditionClass;
                break;
            }

            mrb_value args[] = {di, props, ts};
            auto res = mrb_obj_new(mrb, klass, 3, args);

            return res;
          },
          MRB_ARGS_ARG(2, 1));

      mrb_define_method(
          mrb, observationClass, "initialize",
          [](mrb_state *mrb, mrb_value self) {
            using namespace device_model::data_item;

            mrb_value di;
            mrb_value props;
            mrb_value ts;

            ErrorList errors;
            Timestamp time;

            auto count = mrb_get_args(mrb, "oo|o", &di, &props, &ts);

            auto dataItem = MRubySharedPtr<Entity>::unwrap<DataItem>(mrb, di);

            if (count < 3)
              time = std::chrono::system_clock::now();
            else
              time = timestampFromRuby(mrb, ts);

            Properties values;
            fromRuby(mrb, props, values);
            ObservationPtr obs = Observation::make(dataItem, values, time, errors);

            if (errors.size() > 0)
            {
              ostringstream str;
              for (auto &e : errors)
              {
                str << e->what() << ", ";
              }

              mrb_raise(mrb, E_ARGUMENT_ERROR, str.str().c_str());
            }

            MRubySharedPtr<Entity>::replace(mrb, self, obs);

            return self;
          },
          MRB_ARGS_ARG(2, 1));

      mrb_define_method(
          mrb, observationClass, "dup",
          [](mrb_state *mrb, mrb_value self) {
            ObservationPtr old = MRubySharedPtr<Entity>::unwrap<Observation>(mrb, self);
            RClass *klass = mrb_class(mrb, self);

            auto dup = old->copy();
            return MRubySharedPtr<Entity>::wrap(mrb, klass, dup);
          },
          MRB_ARGS_NONE());
      mrb_alias_method(mrb, observationClass, mrb_intern_lit(mrb, "copy"),
                       mrb_intern_lit(mrb, "dup"));
      mrb_define_method(
          mrb, observationClass, "data_item",
          [](mrb_state *mrb, mrb_value self) {
            ObservationPtr obs = MRubySharedPtr<Entity>::unwrap<Observation>(mrb, self);
            if (obs->isOrphan())
              return mrb_nil_value();
            else
              return MRubySharedPtr<Entity>::wrap(mrb, "DataItem", obs->getDataItem());
          },
          MRB_ARGS_NONE());

      mrb_define_method(
          mrb, observationClass, "timestamp",
          [](mrb_state *mrb, mrb_value self) {
            ObservationPtr obs = MRubySharedPtr<Entity>::unwrap<Observation>(mrb, self);
            return toRuby(mrb, obs->getTimestamp());
          },
          MRB_ARGS_NONE());

      mrb_define_method(
          mrb, m_conditionClass, "level",
          [](mrb_state *mrb, mrb_value self) {
            ObservationPtr obs = MRubySharedPtr<Entity>::unwrap<Observation>(mrb, self);
            auto cond = std::dynamic_pointer_cast<Condition>(obs);
            mrb_value level;
            using namespace observation;
            switch (cond->getLevel())
            {
              case Condition::NORMAL:
                level = mrb_str_new_cstr(mrb, "normal");
                break;

              case Condition::WARNING:
                level = mrb_str_new_cstr(mrb, "warning");
                break;

              case Condition::FAULT:
                level = mrb_str_new_cstr(mrb, "fault");
                break;

              case Condition::UNAVAILABLE:
                level = mrb_str_new_cstr(mrb, "unavailable");
                break;
            }
            return level;
          },
          MRB_ARGS_NONE());

      mrb_define_method(
          mrb, m_conditionClass, "level=",
          [](mrb_state *mrb, mrb_value self) {
            ObservationPtr obs = MRubySharedPtr<Entity>::unwrap<Observation>(mrb, self);
            auto cond = std::dynamic_pointer_cast<Condition>(obs);
            const char *arg = nullptr;
            mrb_get_args(mrb, "z!", &arg);
            if (arg == nullptr)
              return mrb_nil_value();

            cond->setLevel(arg);

            return mrb_str_new_cstr(mrb, arg);
          },
          MRB_ARGS_REQ(1));
    }
  };

  /// @struct RubyObservation
  /// @remark Ruby Observation Wrapper
  /// @code
  /// class Observation -> mtconnect::observation::Observation
  ///   def self.make(data_item, properties, timestamp = now) -> Observation::make(...)
  ///   def initialize(data_item, properties, timestamp = now) -> Observation::make(...)
  ///   def dup -> mtconnect::observation::Observation::copy()
  ///   def data_item -> mtconnect::observation::Observation::getDataItem()
  ///   def timestamp -> mtconnect::observation::Observation::getTimestamp()
  /// end
  ///
  /// class Condition -> mtconnect::observation::Condition
  ///   def level -> mtconnect::observation::Condition::getLevel()
  ///   def level=(level) -> mtconnect::observation::Condition::setLevel(level)
  /// end
  /// @endcode
  ///
  /// @note Both `make` and `initialize` go through Observation::make, which
  ///       decides whether the observation is unavailable before the object is
  ///       built. Two things follow that are easy to trip over from ruby:
  ///
  ///       1. An `UNAVAILABLE` value marks the observation unavailable and is
  ///          then erased, so the observation does not carry the literal text.
  ///          The match is case insensitive, and it is the `level` property for
  ///          a condition and `VALUE` for everything else.
  ///       2. **Omitting the property has the same effect.** A condition with no
  ///          `level`, or any other observation with no `VALUE`, is silently
  ///          unavailable rather than an error. A transform that means to report
  ///          a value and passes an empty properties hash by mistake produces an
  ///          unavailable observation, not a failure, so the mistake shows up as
  ///          missing data rather than as a raised exception.
  ///
  ///       See Observation::make in observation.cpp.
}  // namespace mtconnect::ruby
