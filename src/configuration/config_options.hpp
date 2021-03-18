//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

namespace mtconnect
{
  namespace configuration
  {
#define DECLARE_CONFIGURATION(name) inline const char *name = #name;

    // Agent Configuration
    DECLARE_CONFIGURATION(Port);
    DECLARE_CONFIGURATION(ServerIp);
    DECLARE_CONFIGURATION(BufferSize);
    DECLARE_CONFIGURATION(MaxAssets);
    DECLARE_CONFIGURATION(CheckpointFrequency);
    DECLARE_CONFIGURATION(MonitorConfigFiles);
    DECLARE_CONFIGURATION(MinimumConfigReloadAge);
    DECLARE_CONFIGURATION(Pretty);
    DECLARE_CONFIGURATION(PidFile);
    DECLARE_CONFIGURATION(Devices);
    DECLARE_CONFIGURATION(ServiceName);
    DECLARE_CONFIGURATION(SchemaVersion);
    DECLARE_CONFIGURATION(AllowPut);
    DECLARE_CONFIGURATION(AllowPutFrom);
    DECLARE_CONFIGURATION(HttpHeaders);
    DECLARE_CONFIGURATION(LogStreams);
    DECLARE_CONFIGURATION(WorkerThreads);

    // Adapter Configuration
    DECLARE_CONFIGURATION(Device);
    DECLARE_CONFIGURATION(Host);
    DECLARE_CONFIGURATION(PreserveUUID);
    DECLARE_CONFIGURATION(UUID);
    DECLARE_CONFIGURATION(Manufacturer);
    DECLARE_CONFIGURATION(Station);
    DECLARE_CONFIGURATION(SerialNumber);
    DECLARE_CONFIGURATION(FilterDuplicates);
    DECLARE_CONFIGURATION(AutoAvailable);
    DECLARE_CONFIGURATION(IgnoreTimestamps);
    DECLARE_CONFIGURATION(ConversionRequired);
    DECLARE_CONFIGURATION(UpcaseDataItemValue);
    DECLARE_CONFIGURATION(RealTime);
    DECLARE_CONFIGURATION(RelativeTime);
    DECLARE_CONFIGURATION(ShdrVersion);
    DECLARE_CONFIGURATION(ReconnectInterval);
    DECLARE_CONFIGURATION(LegacyTimeout);
    DECLARE_CONFIGURATION(AdditionalDevices);
    DECLARE_CONFIGURATION(AdapterIdentity);
  }  // namespace configuration
}  // namespace mtconnect
