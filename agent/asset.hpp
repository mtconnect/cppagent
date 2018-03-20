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

#include <string>
#include <map>
#include "ref_counted.hpp"

class Asset;
typedef RefCountedPtr<Asset> AssetPtr;

// An association of the index type to the value.
typedef std::map<std::string, std::string> AssetKeys;
typedef std::map<std::string, std::string> XmlAttributes;

class Asset : public RefCounted
{
protected:
	std::string m_assetId;
	std::string m_content;
	std::string m_type;
	std::string m_deviceUuid;
	std::string m_timestamp;
	std::string m_description;
	XmlAttributes m_archetype;

	bool        m_removed;
	AssetKeys   m_keys;
	AssetKeys   m_identity;

public:

	Asset(const Asset &another)
	{
		m_assetId = another.m_assetId;
		m_content = another.m_content;
		m_type = another.m_type;
		m_removed = another.m_removed;
	}

	Asset(const std::string &assetId,
		const std::string &type,
		const std::string &content,
		const bool removed = false);

	virtual ~Asset();

	const std::string &getAssetId() const {
		return m_assetId; }
	virtual std::string &getContent() {
		return m_content; }
	const std::string &getType() const {
		return m_type; }
	AssetKeys &getKeys() {
		return m_keys; }
	const std::string &getDeviceUuid() const {
		return m_deviceUuid; }
	const std::string &getTimestamp() const {
		return m_timestamp; }
	const std::string &getDescription() const {
		return m_description; }
	const XmlAttributes &getArchetype() const {
		return m_archetype; }
	bool isRemoved() const {
		return m_removed; }

	AssetKeys &getIdentity() {
		return m_identity; }

	bool operator==(const Asset &another) const {
		return m_assetId == another.m_assetId;
	}

	void setAssetId(const std::string &id) {
		m_assetId = id; }
	void setDeviceUuid(const std::string &uuid) {
		m_deviceUuid = uuid; }
	void setTimestamp(const std::string &timestamp) {
		m_timestamp = timestamp; }
	void setRemoved(bool removed) {
		m_removed = removed; }
	void setDescription(const std::string &desc) {
		m_description = desc; }
	void setArchetype(const XmlAttributes &arch) {
		m_archetype = arch; }

	virtual void changed() { }
	virtual void addIdentity(const std::string &key, const std::string &value);
};

typedef std::map<std::string, AssetPtr> AssetIndex;

