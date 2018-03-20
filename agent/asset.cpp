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

Asset::Asset(
	const std::string &asssetId,
	const std::string &type,
	const std::string &content,
	const bool removed) : 
	m_assetId(asssetId),
	m_content(content),
	m_type(type),
	m_removed(removed)
{
}


Asset::~Asset()
{
}


void Asset::addIdentity(const std::string &key, const std::string &value)
{
	if (key == "deviceUuid")
		m_deviceUuid = value;
	else if (key == "timestamp")
		m_timestamp = value;
	else if (key == "removed")
		m_removed = value == "true";
	else if (key == "assetId")
		m_assetId = value;
	else
		m_identity[key] = value;
}

