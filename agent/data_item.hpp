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

#include <map>

#include "dlib/threads.h"

#include "component.hpp"
#include "globals.hpp"
#include "change_observer.hpp"

#ifdef PASCAL
	#undef PASCAL
#endif

class Adapter;

class DataItem : public ChangeSignaler
{
public:
	/* Enumeration for data item category */
	enum ECategory
	{
	SAMPLE,
	EVENT,
	CONDITION
	};

	enum ERepresentation
	{
	VALUE,
	TIME_SERIES,
	DISCRETE
	};

	enum EFilterType
	{
	FILTER_MINIMUM_DELTA,
	FILTER_PERIOD,
	FILTER_NONE
	};


	/* Enumeration for the simple units for simple conversions */
	enum ESimpleUnits
	{
	AMPERE,
	COUNT,
	JOULE,
	PASCAL,
	PH,
	VOLT,
	WATT,
	OHM,
	SOUND_LEVEL,
	SIEMENS,
	DECIBEL,
	INCH,
	FOOT,
	CENTIMETER,
	DECIMETER,
	METER,
	FAHRENHEIT,
	POUND,
	GRAM,
	RADIAN,
	MINUTE,
	HOUR,
	SECOND,
	MILLIMETER,
	LITER,
	DEGREE,
	KILOGRAM,
	NEWTON,
	CELSIUS,
	REVOLUTION,
	STATUS,
	PERCENT,
	NEWTON_MILLIMETER,
	HERTZ,
	MILLIMETER_3D,
	DEGREE_3D
	};

	static const unsigned int NumSimpleUnits = 36;
	static const std::string SSimpleUnits[];

public:
	/* Construct a data item with appropriate attributes mapping */
	DataItem(std::map<std::string, std::string> attributes);

	/* Destructor */
	~DataItem();

	/* Get a map of all the attributes of this data item */
	std::map<std::string, std::string> *getAttributes() { return &m_attributes; }

	/* Getter methods for data item specs */
	const std::string &getId() const { return m_id; }
	const std::string &getName() const { return m_name; }
	const std::string &getSource() const { return m_source; }
	const std::string &getType() const { return m_type; }
	const std::string &getElementName() const { return m_camelType; }
	const std::string &getPrefixedElementName() const { return m_prefixedCamelType; }
	const std::string &getSubType() const { return m_subType; }
	const std::string &getNativeUnits() const { return m_nativeUnits; }
	const std::string &getUnits() const { return m_units; }
	const std::string &getPrefix() const { return m_prefix; }
	const std::string &getStatistic() const { return m_statistic; }
	const std::string &getSampleRate() const { return m_sampleRate; }
	const std::string &getCompositionId() const { return m_compositionId; }

	float getNativeScale() const { return m_nativeScale; }
	double getConversionFactor() const { return m_conversionFactor; }
	double getConversionOffset() const { return m_conversionOffset; }
	bool hasFactor() const { return m_hasFactor; }
	ECategory getCategory() const { return m_category; }
	ERepresentation getRepresentation() const { return m_representation; }

	void setConversionFactor(double aFactor, double aOffset);

	/* Returns if data item has this attribute */
	bool hasName(const std::string &name);
	bool hasNativeScale() const { return m_hasNativeScale; }

	/* Add a source (extra information) to data item */
	void addSource(const std::string &source) { m_source = source; }

	/* Returns true if data item is a sample */
	bool isSample() const { return m_category == SAMPLE; }
	bool isEvent() const { return m_category == EVENT; }
	bool isCondition() const { return m_category == CONDITION; }
	bool isAlarm() const { return m_isAlarm; }
	bool isMessage() const { return m_isMessage; }
	bool isAssetChanged() const { return m_isAssetChanged; }
	bool isAssetRemoved() const { return m_isAssetRemoved; }
	bool isTimeSeries() const { return m_representation == TIME_SERIES; }
	bool isDiscrete() const { return m_representation == DISCRETE; }

	bool hasResetTrigger() const { return !m_resetTrigger.empty(); }
	const std::string &getResetTrigger() const { return m_resetTrigger; }
	void setResetTrigger(const std::string &aTrigger) { m_resetTrigger = aTrigger; }

	bool hasInitialValue() const { return !m_initialValue.empty(); }
	const std::string &getInitialValue() const { return m_initialValue; }
	void setInitialValue(const std::string &aValue) { m_initialValue = aValue; }


	/* Set/get component that data item is associated with */
	void setComponent(Component &component) { m_component = &component; }
	Component *getComponent() const { return m_component; }

	/* Get the name for the adapter feed */
	std::string getSourceOrName() { return m_source.empty() ? (m_name.empty() ? m_id : m_name) : m_source; }

	/* Transform a name to camel casing */
	static std::string getCamelType(const std::string &aType,
					std::string &aPrefix);

	/* Duplicate Checking */
	bool isDuplicate(const std::string &aValue)
	{
	// Do not dupe check for time series.
	if (m_representation != VALUE)
		return false;
	else if (aValue == m_lastValue)
		return true;

	m_lastValue = aValue;
	return false;
	}

	/* Filter checking */
	bool isFiltered(const double aValue, const double aTimeOffset)
	{
	if (m_hasMinimumDelta && m_category == SAMPLE)
	{
		if (!ISNAN(m_lastSampleValue))
		{
		if (aValue > (m_lastSampleValue - m_filterValue) && aValue < (m_lastSampleValue + m_filterValue))
		{
			// Filter value
			return true;
		}
		}

		m_lastSampleValue = aValue;
	}

	if (m_hasMinimumPeriod)
	{
		if (!ISNAN(m_lastTimeOffset) && !ISNAN(aTimeOffset))
		{
		if (aTimeOffset < (m_lastTimeOffset + m_filterPeriod))
		{
			// Filter value
			return true;
		}
		}

		m_lastTimeOffset = aTimeOffset;
	}

	return false;
	}

	/* Constrainsts */
	bool hasConstraints() { return m_hasConstraints; }
	std::string getMaximum() const { return m_maximum; }
	std::string getMinimum() const { return m_minimum; }
	std::vector<std::string> &getConstrainedValues() { return m_values; }
	bool hasConstantValue() { return m_values.size() == 1; }

	bool hasMinimumDelta() const { return m_hasMinimumDelta; }
	bool hasMinimumPeriod() const { return m_hasMinimumPeriod; }
	double getFilterValue() const { return m_filterValue; }
	double getFilterPeriod() const { return m_filterPeriod; }

	void setMaximum(std::string aMax) { m_maximum = aMax; m_hasConstraints = true; }
	void setMinimum(std::string aMin) { m_minimum = aMin; m_hasConstraints = true; }
	void addConstrainedValue(std::string aValue) { m_values.push_back(aValue); m_hasConstraints = true; }

	void setMinmumDelta(double aValue) { m_filterValue = aValue; m_hasMinimumDelta = true; }
	void setMinmumPeriod(double aValue) { m_filterPeriod = aValue; m_hasMinimumPeriod = true; }

	bool conversionRequired();
	std::string convertValue(const std::string &value);
	float convertValue(float aValue);

	Adapter *getDataSource() const { return m_dataSource;  }
	void setDataSource(Adapter *aSource);
	bool operator<(DataItem &aOther);

	bool operator==(DataItem &aOther)
	{
	return m_id == aOther.m_id;
	}

protected:
	double simpleFactor(const std::string &units);
	std::map<std::string, std::string> buildAttributes() const;
	void computeConversionFactors();

protected:
	/* Unique ID for each component */
	std::string m_id;

	/* Name for itself */
	std::string m_name;

	/* Type of data item */
	std::string m_type;
	std::string m_camelType;
	std::string m_prefixedCamelType;
	std::string m_prefix;

	/* Subtype of data item */
	std::string m_subType;

	/* Category of data item */
	ECategory m_category;

	/* Native units of data item */
	std::string m_nativeUnits;

	/* Units of data item */
	std::string m_units;

	/* The statistical process used on this data item */
	std::string m_statistic;

	/* Representation of data item */
	ERepresentation m_representation;
	std::string m_sampleRate;

	std::string m_compositionId;

	/* Native scale of data item */
	float m_nativeScale;
	bool m_hasNativeScale;
	bool m_threeD;
	bool m_isMessage, m_isAlarm, m_isAssetChanged, m_isAssetRemoved;

	/* Sig figs of data item */
	unsigned int m_significantDigits;
	bool m_hasSignificantDigits;

	/* Coordinate system of data item */
	std::string m_coordinateSystem;

	/* Extra source information of data item */
	std::string m_source;

	// The reset trigger;
	std::string m_resetTrigger;

	// Initial value
	std::string m_initialValue;

	/* Constraints for this data item */
	std::string m_maximum, m_minimum;
	std::vector<std::string> m_values;
	bool m_hasConstraints;

	double m_filterValue;
  	// Period filter, in seconds
	double m_filterPeriod;
	bool m_hasMinimumDelta;
	bool m_hasMinimumPeriod;

	/* Component that data item is associated with */
	Component *m_component;

	/* Duplicate and filter checking */
	std::string m_lastValue;
	double m_lastSampleValue;
	double m_lastTimeOffset;

	/* Attrubutes */
	std::map<std::string, std::string> m_attributes;

	/* The data source for this data item */
	Adapter *m_dataSource;

	/* Conversion factor */
	double m_conversionFactor;
	double m_conversionOffset;
	bool m_conversionDetermined, m_conversionRequired, m_hasFactor;
};
