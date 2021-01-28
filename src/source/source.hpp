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

#include "entity/entity.hpp"

namespace mtconnect
{
  namespace source
  {
    // A source manages a data source and the transformations to an
    // destination. The transformations are linked together. Source is
    // a state monad managing the other monads that perform the
    // transformtion on the Entities.

    class Source
    {
    public:
    };
  }  // namespace source
}  // namespace mtconnect
