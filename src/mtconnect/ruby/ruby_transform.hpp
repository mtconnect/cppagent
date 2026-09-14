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

#include "mtconnect/config.hpp"
#include "mtconnect/pipeline/guard.hpp"
#include "mtconnect/pipeline/topic_mapper.hpp"
#include "mtconnect/pipeline/transform.hpp"
#include "ruby_entity.hpp"
#include "ruby_observation.hpp"
#include "ruby_pipeline.hpp"

namespace mtconnect::ruby {
  using namespace mtconnect::pipeline;
  using namespace std::literals;
  using namespace entity;
  using namespace observation;

  class AGENT_LIB_API RubyTransform : public pipeline::Transform
  {
  public:
    static void initialize(mrb_state* mrb, RClass* module)
    {
      using namespace std;

      auto transClass = mrb_define_class_under(mrb, module, "Transform", mrb->object_class);
      MRB_SET_INSTANCE_TT(transClass, MRB_TT_DATA);
      mrb_mod_cv_set(mrb, transClass, mrb_intern_check_cstr(mrb, "CONTINUE"),
                     mrb_int_value(mrb, CONTINUE));
      mrb_mod_cv_set(mrb, transClass, mrb_intern_check_cstr(mrb, "RUN"), mrb_int_value(mrb, RUN));
      mrb_mod_cv_set(mrb, transClass, mrb_intern_check_cstr(mrb, "SKIP"), mrb_int_value(mrb, SKIP));

      mrb_define_method(
          mrb, transClass, "transform",
          [](mrb_state* mrb, mrb_value self) {
            auto trans = MRubySharedPtr<Transform>::unwrap(mrb, self);

            EntityPtr* ent;
            mrb_get_args(mrb, "d", &ent, MRubySharedPtr<Entity>::type());
            auto r = (*trans)(std::move(*ent));
            return MRubySharedPtr<Entity>::wrap(mrb, "Entity", r);
          },
          MRB_ARGS_REQ(1));

      auto rubyTrans = mrb_define_class_under(mrb, module, "RubyTransform", transClass);
      MRB_SET_INSTANCE_TT(transClass, MRB_TT_DATA);

      mrb_define_method(
          mrb, rubyTrans, "initialize",
          [](mrb_state* mrb, mrb_value self) {
            const char* name;
            mrb_value gv, block = mrb_nil_value();
            string guard;

            auto c = mrb_get_args(mrb, "z|o&", &name, &gv, &block);
            if (c == 1)
              guard = "Entity";
            else
              guard = stringFromRuby(mrb, gv);

            auto trans = make_shared<RubyTransform>(mrb, self, name, guard);
            if (mrb_block_given_p(mrb))
            {
              trans->m_block = block;
              mrb_gc_register(mrb, block);
            }
            MRubySharedPtr<Transform>::replace(mrb, self, trans);

            return self;
          },
          MRB_ARGS_ARG(1, 1) | MRB_ARGS_BLOCK());

      mrb_define_method(
          mrb, rubyTrans, "forward",
          [](mrb_state* mrb, mrb_value self) {
            auto trans = MRubySharedPtr<Transform>::unwrap<RubyTransform>(mrb, self);

            EntityPtr* ent;
            mrb_get_args(mrb, "d", &ent, MRubySharedPtr<Entity>::type());
            auto nxt = trans->next(EntityPtr(*ent));
            return MRubySharedPtr<Entity>::wrap(mrb, "Entity", nxt);
          },
          MRB_ARGS_REQ(1));

      mrb_define_method(
          mrb, rubyTrans, "bind",
          [](mrb_state* mrb, mrb_value self) {
            auto trans = MRubySharedPtr<Transform>::unwrap<RubyTransform>(mrb, self);

            TransformPtr* to;
            mrb_get_args(mrb, "d", &to, MRubySharedPtr<Transform>::type());
            auto nxt = trans->bind(*to);
            return MRubySharedPtr<Transform>::wrap(mrb, "Transform", nxt);
          },
          MRB_ARGS_REQ(1));

      mrb_define_method(
          mrb, rubyTrans, "guard=",
          [](mrb_state* mrb, mrb_value self) {
            auto trans = MRubySharedPtr<Transform>::unwrap<RubyTransform>(mrb, self);
            const char* guard;
            if (mrb_get_args(mrb, "z", &guard) > 0)
            {
              trans->m_guardString = guard;
            }

            trans->setGuard();

            return self;
          },
          MRB_ARGS_OPT(1));

      mrb_define_method(
          mrb, rubyTrans, "guard",
          [](mrb_state* mrb, mrb_value self) {
            auto trans = MRubySharedPtr<Transform>::unwrap<RubyTransform>(mrb, self);
            const char* guard;
            mrb_value block = mrb_nil_value();
            if (mrb_block_given_p(mrb))
            {
              mrb_get_args(mrb, "&", &block);
              if (!mrb_nil_p(block))
              {
                trans->m_guardBlock = block;
                mrb_gc_register(mrb, block);
              }
            }
            else if (mrb_get_args(mrb, "z", &guard) > 0)
            {
              trans->m_guardString = guard;
            }

            trans->setGuard();

            return self;
          },
          MRB_ARGS_OPT(1) | MRB_ARGS_BLOCK());
    }

    RubyTransform(mrb_state* mrb, mrb_value self, const std::string& name, const std::string& guard)
      : Transform(name),
        m_self(self),
        m_method(mrb_intern_lit(mrb, "transform")),
        m_block(mrb_nil_value()),
        m_guardString(guard),
        m_guardBlock(mrb_nil_value())
    {
      setGuard();
    }

    ~RubyTransform()
    {
      std::lock_guard guard(RubyVM::rubyVM());
      if (RubyVM::hasVM())
      {
        auto mrb = RubyVM::rubyVM().state();

        mrb_gc_unregister(mrb, m_self);
        m_self = mrb_nil_value();
        if (!mrb_nil_p(m_block))
          mrb_gc_unregister(mrb, m_block);
        if (!mrb_nil_p(m_guardBlock))
          mrb_gc_unregister(mrb, m_guardBlock);
        m_block = mrb_nil_value();
        m_guardBlock = mrb_nil_value();
      }
    }

    void setMethod(mrb_sym sym) { m_method = sym; }

    void setGuard()
    {
      if (!mrb_nil_p(m_guardBlock))
      {
        m_guard = [this, old = m_guard](const entity::Entity* entity) -> GuardAction {
          using namespace entity;
          using namespace observation;
          std::lock_guard guard(RubyVM::rubyVM());

          auto mrb = RubyVM::rubyVM().state();
          int save = mrb_gc_arena_save(mrb);

          entity::EntityPtr ptr = entity->getptr();
          mrb_value ev = MRubySharedPtr<Entity>::wrap(mrb, "Entity", ptr);

          mrb_bool state = false;
          mrb_value values[] = {m_guardBlock, ev};
          mrb_value data = mrb_ary_new_from_values(mrb, 2, values);
          mrb_value rv = mrb_protect(
              mrb,
              [](mrb_state* mrb, mrb_value data) {
                mrb_value block = mrb_ary_ref(mrb, data, 0);
                mrb_value ev = mrb_ary_ref(mrb, data, 1);

                return mrb_yield(mrb, block, ev);
              },
              data, &state);

          if (state)
          {
            LOG(error) << "Error in guard: " << mrb_str_to_cstr(mrb, mrb_inspect(mrb, rv));
            rv = mrb_nil_value();
          }

          mrb_gc_arena_restore(mrb, save);
          if (!mrb_nil_p(rv))
          {
            auto s = stringFromRuby(mrb, rv);
            if (s == "RUN")
              return GuardAction::RUN;
            else if (s == "SKIP")
              return GuardAction::SKIP;
            else
              return GuardAction::CONTINUE;
          }
          else
          {
            return old(entity);
          }
        };
      }
      else if (m_guardString == "Observation")
        m_guard = TypeGuard<Observation>(RUN) || GuardCls(SKIP);
      else if (m_guardString == "Sample")
        m_guard = TypeGuard<Sample>(RUN) || GuardCls(SKIP);
      else if (m_guardString == "Event")
        m_guard = TypeGuard<Event>(RUN) || GuardCls(SKIP);
      else if (m_guardString == "Condition")
        m_guard = TypeGuard<Condition>(RUN) || GuardCls(SKIP);
      else if (m_guardString == "Tokens")
        m_guard = TypeGuard<pipeline::Tokens>(RUN) || GuardCls(SKIP);
      else if (m_guardString == "Message")
        m_guard = TypeGuard<PipelineMessage>(RUN) || GuardCls(SKIP);
      else
        m_guard = GuardCls(RUN);
    }

    using calldata = std::pair<RubyTransform*, EntityPtr>;

    /// @brief Is the ruby value a wrapped MTConnect entity (C data of the Entity type)?
    /// @note Does not raise, unlike mrb_data_get_ptr, so it is safe to call outside mrb_protect.
    static bool isEntity(mrb_state *mrb, mrb_value value)
    {
      return mrb_data_p(value) && DATA_TYPE(value) == MRubySharedPtr<Entity>::type();
    }

    entity::EntityPtr operator()(entity::EntityPtr&& entity) override
    {
      NAMED_SCOPE("RubyTransform::operator()");

      using namespace entity;
      using namespace observation;

      EntityPtr res;

      std::lock_guard guard(RubyVM::rubyVM());
      auto mrb = RubyVM::rubyVM().state();
      int save = mrb_gc_arena_save(mrb);

      try
      {
        mrb_value ev;
        const char* klass = "Entity";
        Entity* ptr = entity.get();
        Observation* obs;
        if (obs = dynamic_cast<Observation*>(ptr); obs != nullptr)
        {
          switch (obs->getDataItem()->getCategory())
          {
            case device_model::data_item::DataItem::SAMPLE:
              klass = "Sample";
              break;
            case device_model::data_item::DataItem::EVENT:
              klass = "Event";
              break;
            case device_model::data_item::DataItem::CONDITION:
              klass = "Condition";
              break;
          }
        }
        else if (dynamic_cast<pipeline::Timestamped*>(ptr) != nullptr)
          klass = "Timestamped";
        else if (dynamic_cast<pipeline::Tokens*>(ptr) != nullptr)
          klass = "Tokens";

        ev = MRubySharedPtr<Entity>::wrap(mrb, klass, entity);
        mrb_value rv;

        mrb_bool state = false;
        if (!mrb_nil_p(m_block))
        {
          mrb_value values[] = {m_self, m_block, ev};
          mrb_value data = mrb_ary_new_from_values(mrb, 3, values);
          rv = mrb_protect(
              mrb,
              [](mrb_state* mrb, mrb_value data) {
                mrb_value self = mrb_ary_ref(mrb, data, 0);
                mrb_value block = mrb_ary_ref(mrb, data, 1);
                mrb_value ev = mrb_ary_ref(mrb, data, 2);
                return mrb_yield_with_class(mrb, block, 1, &ev, self, mrb_class(mrb, self));
              },
              data, &state);
        }
        else
        {
          mrb_value values[] = {m_self, mrb_symbol_value(m_method), ev};
          mrb_value data = mrb_ary_new_from_values(mrb, 3, values);
          rv = mrb_protect(
              mrb,
              [](mrb_state* mrb, mrb_value data) {
                mrb_value self = mrb_ary_ref(mrb, data, 0);
                mrb_sym method = mrb_symbol(mrb_ary_ref(mrb, data, 1));
                mrb_value ev = mrb_ary_ref(mrb, data, 2);
                return mrb_funcall_id(mrb, self, method, 1, ev);
              },
              data, &state);
        }
        if (state)
        {
          // auto str = mrb_any_to_s(mrb, rv);
          LOG(error) << "Error in transform: " << mrb_str_to_cstr(mrb, mrb_inspect(mrb, rv));
          if (mrb->exc)
          {
            auto exc = mrb_funcall(mrb, mrb_obj_value(mrb->exc), "to_s", 0);
            LOG(error) << "Error calling transform " << m_name << ": " << RSTRING_CSTR(mrb, exc);
          }
          rv = mrb_nil_value();
        }

        // A ruby transform may return nil, a single Entity, or an Array of
        // entities. Anything else (e.g. a stray Array of non-entities left over
        // from an `each`/`map`) must not reach the raising unwrap, which would
        // reinterpret an mruby word as a shared_ptr<Entity> and smash memory --
        // or abort here, since this runs outside mrb_protect. Check the types
        // without raising and drop a bad return with a warning.
        if (isEntity(mrb, rv))
        {
          res = MRubySharedPtr<Entity>::unwrap<Entity>(mrb, rv);
        }
        else if (mrb_array_p(rv))
        {
          // Collect the entity elements into an EntityList carried on a
          // container entity (mirrors JsonMapper's "JsonEntities" result).
          EntityList list;
          bool ok = true;
          auto len = RARRAY_LEN(rv);
          for (mrb_int i = 0; i < len; i++)
          {
            mrb_value el = mrb_ary_ref(mrb, rv, i);
            if (isEntity(mrb, el))
            {
              list.emplace_back(MRubySharedPtr<Entity>::unwrap<Entity>(mrb, el));
            }
            else
            {
              ok = false;
              LOG(warning) << "Ruby transform " << m_name << " returned an array with a non-entity ("
                           << mrb_obj_classname(mrb, el) << ") at index " << i << " -- ignoring array";
              break;
            }
          }
          if (ok && !list.empty())
          {
            auto entities = std::make_shared<Entity>("Entities");
            entities->setValue(list);
            res = entities;
          }
        }
        else if (!mrb_nil_p(rv))
        {
          LOG(warning) << "Ruby transform " << m_name << " returned a non-entity ("
                       << mrb_obj_classname(mrb, rv)
                       << "); expected an Entity, an array of entities, or nil -- ignoring return value";
        }
      }
      catch (const std::exception& e)
      {
        LOG(error) << "Exception thrown in transform" << e.what();
      }
      catch (...)
      {
        LOG(error) << "Unknown Exception thrown in transform";
      }

      mrb_gc_arena_restore(mrb, save);
      return res;
    }

    auto& object() { return m_self; }
    void setObject(mrb_value obj) { m_self = obj; }

  protected:
    PipelineContract* m_contract;
    mrb_value m_self;
    mrb_sym m_method;
    mrb_value m_block;
    std::string m_guardString;
    mrb_value m_guardBlock;
  };
}  // namespace mtconnect::ruby
