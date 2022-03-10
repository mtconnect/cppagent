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

#include <rice/rice.hpp>
#include <rice/stl.hpp>
#include <ruby/thread.h>

namespace mtconnect::ruby::smart_ptr {
  using namespace Rice;
  
  template <typename T>
  inline void replace(VALUE value, rb_data_type_t* rb_type, std::shared_ptr<T> data, bool isOwner)
  {
    using WrapperType = detail::WrapperSmartPointer<std::shared_ptr, T>;
    
    WrapperType* wrapper = nullptr;
    TypedData_Get_Struct(value, WrapperType, rb_type, wrapper);
    delete wrapper;

    wrapper = new WrapperType(data);
    RTYPEDDATA_DATA(value) = wrapper;
  }

  template<typename T, typename...Arg_Ts>
  class Constructor
  {
  public:
    using SharedType = shared_ptr<T>;
    
    static void construct(VALUE value, Arg_Ts...args)
    {
      SharedType data = make_shared<T>(args...);
      replace<T>(value, Data_Type<T>::rb_type(), data, true);
    }
  };

  //! Special-case Constructor used when defining Directors.
  template<typename T, typename...Arg_Ts>
  class Constructor<T, Object, Arg_Ts...>
  {
    public:
      using SharedType = shared_ptr<T>;

      static void construct(Object self, Arg_Ts...args)
      {
        SharedType data = make_shared<T>(self, args...);
        replace<T>(self.value(), Data_Type<T>::rb_type(), data, true);
      }
  };
  
}
