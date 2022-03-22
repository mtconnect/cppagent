//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

namespace mtconnect::ruby {
  template<typename T>
  class MRubySharedPtr
  {
  public:
    using SharedPtr = std::shared_ptr<T>;

    MRubySharedPtr(mrb_state *mrb, const char *name, SharedPtr obj)
    {
      static mrb_data_type s_type { nullptr, nullptr};
      if (s_type.struct_name == nullptr)
      {
        mruby_type = &s_type;
        s_type.struct_name = name;
        s_type.dfree = [](mrb_state *mrb, void *p) {
          auto sp = static_cast<SharedPtr*>(p);
          delete sp;
        };
      }
      
      auto mod = mrb_module_get(mrb, "MTConnect");
      auto klass = mrb_class_get_under(mrb, mod, s_type.struct_name);
      auto ptr = new SharedPtr(obj);

      auto wrapper = mrb_data_object_alloc(mrb, klass, ptr, &s_type);
      m_obj = mrb_obj_value(wrapper);
    }
        
    mrb_value m_obj;
    static mrb_data_type *mruby_type;
  };
  
  template<typename T>
  mrb_data_type *MRubySharedPtr<T>::mruby_type = nullptr;

}
