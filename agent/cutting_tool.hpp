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


#pragma once

#include "asset.hpp"
#include <vector>
#include <map>

class CuttingTool;
typedef RefCountedPtr<CuttingTool> CuttingToolPtr;

class CuttingToolValue;
typedef RefCountedPtr<CuttingToolValue> CuttingToolValuePtr;

class CuttingItem;
typedef RefCountedPtr<CuttingItem> CuttingItemPtr;


class CuttingToolValue : public RefCounted
{
public:
	CuttingToolValue(const std::string &aKey, const std::string &aValue) :
		m_key(aKey),
		m_value(aValue)
	{
	}

	CuttingToolValue()
	{
	}

	CuttingToolValue(const CuttingToolValue &another) :
		m_properties(another.m_properties),
		m_key(another.m_key),
		m_value(another.m_value)
	{
	}

	virtual ~CuttingToolValue();

public:
	std::map<std::string, std::string> m_properties;
	std::string m_key;
	std::string m_value;
};


class CuttingItem : public RefCounted
{
public:
	virtual ~CuttingItem();

public:
	std::map<std::string, std::string> m_identity;
	std::map<std::string, CuttingToolValuePtr> m_values;
	std::map<std::string, CuttingToolValuePtr> m_measurements;
	std::vector<CuttingToolValuePtr> m_lives;
};


class CuttingTool : public Asset
{
public:
	CuttingTool(
		const std::string &assetId,
		const std::string &type,
		const std::string &content,
		bool removed = false)
		:
		Asset(assetId, type, content, removed)
	{
	}
	~CuttingTool();

	virtual void addIdentity(const std::string &key, const std::string &value);
	void addValue(const CuttingToolValuePtr value);
	void updateValue(const std::string &key, const std::string &value);

	virtual std::string &getContent();
	virtual void changed() {
		m_content.clear(); }

public:
	std::vector<std::string> m_status;
	std::map<std::string, CuttingToolValuePtr> m_values;
	std::map<std::string, CuttingToolValuePtr> m_measurements;
	std::string m_itemCount;
	std::vector<CuttingItemPtr> m_items;
	std::vector<CuttingToolValuePtr> m_lives;
};
