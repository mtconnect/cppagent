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

#pragma once

#include "observation/observation.hpp"
#include "ruby_entity.hpp"
#include "ruby_vm.hpp"

namespace mtconnect::ruby {
  using namespace mtconnect::observation;
  using namespace std;
  using namespace Rice;

  struct RubyObservation
  {
    static void initialize(mrb_state *mrb, RClass *module)
    {
      auto entityClass = mrb_class_get_under(mrb, module, "Entity");
      auto observationClass = mrb_define_class_under(mrb, module, "Observation", entityClass);
      MRB_SET_INSTANCE_TT(observationClass, MRB_TT_DATA);

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

      auto eventClass = mrb_define_class_under(mrb, module, "Event", observationClass);
      MRB_SET_INSTANCE_TT(eventClass, MRB_TT_DATA);

      auto sampleClass = mrb_define_class_under(mrb, module, "Sample", observationClass);
      MRB_SET_INSTANCE_TT(sampleClass, MRB_TT_DATA);

      auto conditionClass = mrb_define_class_under(mrb, module, "Condition", observationClass);
      MRB_SET_INSTANCE_TT(conditionClass, MRB_TT_DATA);
    }
  };
}  // namespace mtconnect::ruby
