//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

namespace mtconnect::ruby {
  template <typename T>
  struct MRubySharedPtr
  {
    using SharedPtr = std::shared_ptr<T>;

    AGENT_SYMBOL_VISIBLE static mrb_data_type *type()
    {
      static mrb_data_type s_type {nullptr, nullptr};
      if (s_type.struct_name == nullptr)
      {
        mruby_type = &s_type;
        s_type.struct_name = typeid(T).name();
        s_type.dfree = [](mrb_state *mrb, void *p) {
          auto sp = static_cast<SharedPtr *>(p);
          delete sp;
        };
      }

      return &s_type;
    }

    static mrb_value wrap(mrb_state *mrb, const char *name, SharedPtr obj)
    {
      if (!obj)
        return mrb_nil_value();

      auto mod = mrb_module_get(mrb, "MTConnect");
      auto klass = mrb_class_get_under(mrb, mod, name);
      auto ptr = new SharedPtr(obj);

      auto wrapper = mrb_data_object_alloc(mrb, klass, ptr, type());
      return mrb_obj_value(wrapper);
    }

    static mrb_value wrap(mrb_state *mrb, RClass *klass, SharedPtr obj)
    {
      if (!obj)
        return mrb_nil_value();

      auto ptr = new SharedPtr(obj);
      auto wrapper = mrb_data_object_alloc(mrb, klass, ptr, type());
      return mrb_obj_value(wrapper);
    }

    static void replace(mrb_state *mrb, mrb_value self, SharedPtr obj)
    {
      auto selfp = static_cast<SharedPtr *>(DATA_PTR(self));
      if (selfp)
      {
        delete selfp;
      }
      mrb_data_init(self, new SharedPtr(obj), type());
    }

    static SharedPtr unwrap(mrb_state *mrb, mrb_value value)
    {
      void *dp = mrb_data_get_ptr(mrb, value, type());
      if (dp != nullptr)
        return *static_cast<SharedPtr *>(dp);
      else
        return nullptr;
    }

    template <typename U>
    static std::shared_ptr<U> unwrap(mrb_state *mrb, mrb_value value)
    {
      void *dp = mrb_data_get_ptr(mrb, value, type());
      if (dp != nullptr)
      {
        SharedPtr ptr(*static_cast<SharedPtr *>(dp));
        return std::dynamic_pointer_cast<U>(ptr);
      }
      else
        return nullptr;
    }

    static SharedPtr unwrap(mrb_value value)
    {
      void *dp = DATA_PTR(value);
      if (dp != nullptr)
        return *static_cast<SharedPtr *>(dp);
      else
        return nullptr;
    }

    template <typename U>
    static SharedPtr unwrap(mrb_value value)
    {
      void *dp = DATA_PTR(value);
      if (dp != nullptr)
      {
        std::shared_ptr<U> ptr(*static_cast<SharedPtr *>(dp));
        return std::dynamic_pointer_cast<U>(ptr);
      }
      else
        return nullptr;
    }

  private:
    static mrb_data_type *mruby_type;
  };

  template <typename T>
  mrb_data_type *MRubySharedPtr<T>::mruby_type = nullptr;

  template <typename T>
  struct MRubyPtr
  {
    using Ptr = T *;

    AGENT_SYMBOL_VISIBLE static mrb_data_type *type()
    {
      static mrb_data_type s_type {nullptr, nullptr};

      if (s_type.struct_name == nullptr)
      {
        mruby_type = &s_type;
        s_type.struct_name = typeid(T).name();
      }

      return &s_type;
    }

    static mrb_value wrap(mrb_state *mrb, const char *name, Ptr obj)
    {
      if (obj == nullptr)
        return mrb_nil_value();

      auto mod = mrb_module_get(mrb, "MTConnect");
      auto klass = mrb_class_get_under(mrb, mod, name);

      auto wrapper = mrb_data_object_alloc(mrb, klass, obj, type());
      return mrb_obj_value(wrapper);
    }

    static mrb_value wrap(mrb_state *mrb, RClass *klass, Ptr obj)
    {
      if (obj == nullptr)
        return mrb_nil_value();

      auto wrapper = mrb_data_object_alloc(mrb, klass, obj, type());
      return mrb_obj_value(wrapper);
    }

    static Ptr unwrap(mrb_state *mrb, mrb_value value)
    {
      return static_cast<Ptr>(mrb_data_get_ptr(mrb, value, type()));
    }

    static void replace(mrb_state *mrb, mrb_value self, Ptr obj)
    {
      mrb_data_init(self, obj, type());
    }

    static Ptr unwrap(mrb_value value) { return static_cast<Ptr>(DATA_PTR(value)); }

  private:
    static mrb_data_type *mruby_type;
  };

  template <typename T>
  mrb_data_type *MRubyPtr<T>::mruby_type = nullptr;

  template <typename T>
  struct MRubyUniquePtr
  {
    using UniquePtr = std::unique_ptr<T>;

    AGENT_SYMBOL_VISIBLE static mrb_data_type *type()
    {
      static mrb_data_type s_type {nullptr, nullptr};
      if (s_type.struct_name == nullptr)
      {
        mruby_type = &s_type;
        s_type.struct_name = typeid(T).name();
        s_type.dfree = [](mrb_state *mrb, void *p) {
          auto sp = static_cast<UniquePtr *>(p);
          delete sp;
        };
      }

      return &s_type;
    }

    static mrb_value wrap(mrb_state *mrb, const char *name, T *obj)
    {
      if (!obj)
        return mrb_nil_value();

      auto mod = mrb_module_get(mrb, "MTConnect");
      auto klass = mrb_class_get_under(mrb, mod, name);
      auto ptr = new UniquePtr(obj);

      auto wrapper = mrb_data_object_alloc(mrb, klass, ptr, type());
      return mrb_obj_value(wrapper);
    }

    static mrb_value wrap(mrb_state *mrb, RClass *klass, T *obj)
    {
      if (!obj)
        return mrb_nil_value();

      auto ptr = new UniquePtr(obj);
      auto wrapper = mrb_data_object_alloc(mrb, klass, ptr, type());
      return mrb_obj_value(wrapper);
    }

    static void replace(mrb_state *mrb, mrb_value self, UniquePtr obj)
    {
      auto selfp = static_cast<UniquePtr *>(DATA_PTR(self));
      if (selfp)
      {
        delete selfp;
      }
      mrb_data_init(self, new UniquePtr(obj), type());
    }

    static T *unwrap(mrb_state *mrb, mrb_value value)
    {
      UniquePtr *ptr = static_cast<UniquePtr *>(mrb_data_get_ptr(mrb, value, type()));
      return ptr->get();
    }

    static T *unwrap(mrb_value value)
    {
      UniquePtr *ptr = static_cast<UniquePtr *>(DATA_PTR(value));
      return ptr->get();
    }

  private:
    static mrb_data_type *mruby_type;
  };

  template <typename T>
  mrb_data_type *MRubyUniquePtr<T>::mruby_type = nullptr;

}  // namespace mtconnect::ruby
