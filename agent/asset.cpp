//
// Copyright Copyright 2012, System Insights, Inc.
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
#include "asset.hpp"
#include <map>

using namespace std;

Asset::Asset(const std::string &aAssetId, const std::string &aType, const std::string &aContent,
		 const bool aRemoved)
	: m_assetId(aAssetId), m_content(aContent),  m_type(aType), m_description(""), m_removed(aRemoved)
{
}

Asset::~Asset()
{
}

void Asset::addIdentity(const std::string &aKey, const std::string &aValue)
{
	if (aKey == "deviceUuid")
	{
	m_deviceUuid = aValue;
	}
	else if (aKey == "timestamp")
	{
	m_timestamp = aValue;
	}
	else if (aKey == "removed")
	{
	m_removed = aValue == "true";
	}
	else if (aKey == "assetId")
	{
	m_assetId = aValue;
	}
	else
	{
	m_identity[aKey] = aValue;
	}
}

