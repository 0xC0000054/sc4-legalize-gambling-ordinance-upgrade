//////////////////////////////////////////////////////////////////////////////
//
// This file is part of sc4-legalize-gambling-ordinance-upgrade, a DLL Plugin
// for SimCity 4 that updates the built-in ordinance to have its income based
// on the city's residential population.
//
// Copyright (c) 2023 Nicholas Hayes
//
// This file is licensed under terms of the MIT License.
// See LICENSE.txt for more information.
//
//////////////////////////////////////////////////////////////////////////////

#include "SC4BuiltInOrdinanceBase.h"
#include "StringResourceManager.h"
#include "cIGZDate.h"
#include "cIGZIStream.h"
#include "cIGZOStream.h"
#include "cISC4App.h"
#include "cISC4City.h"
#include "cISC4ResidentialSimulator.h"
#include "cISC4Simulator.h"
#include "cRZCOMDllDirector.h"
#include "GZServPtrs.h"
#include <algorithm>
#include <stdlib.h>

static const uint32_t GZIID_SC4BuiltInOrdinanceBase = 0xffec6dfb;

static const uint32_t kSC4CLSID_cSC4ResidentialSimulator = 0x4990C013;
static const uint32_t kSC4CLSID_cSC4Simulator = 0x2990C1E5;

static const uint32_t GZIID_cISC4ResidentialSimulator = 0x77ac1ee;
static const uint32_t GZIID_cISC4Simulator = 0x8695664e;

static const uint32_t kExemplarTypeID = 0x6534284a;

namespace
{
	bool ReadSC4BuiltInOrdnanceProperties(cIGZIStream& stream, BuiltInOrdinanaceExemplarInfo& exemplarInfo)
	{
		// SC4's built-in ordinances use the following property format:
		// Uint16 - Exemplar property data version (always 2)
		// Uint16 - Generic property data version (always 2).
		// Uint32 - Generic property count (always 0).
		// Uint32 - Ordinance Exemplar Group ID.
		// Uint32 - Ordinance Exemplar Type ID.
		// Uint32 - Ordinance Exemplar Instance ID.

		uint16_t exemplarPropertyDataVersion = 0;
		if (!stream.GetUint16(exemplarPropertyDataVersion) || exemplarPropertyDataVersion != 2)
		{
			return false;
		}

		uint16_t genericPropertyDataVersion = 0;
		if (!stream.GetUint16(genericPropertyDataVersion) || genericPropertyDataVersion != 2)
		{
			return false;
		}

		uint32_t genericPropertyCount = 0;
		if (!stream.GetUint32(genericPropertyCount) || genericPropertyCount != 0)
		{
			return false;
		}

		if (!stream.GetUint32(exemplarInfo.group))
		{
			return false;
		}

		uint32_t exemplarType = 0;
		if (!stream.GetUint32(exemplarType) || exemplarType != kExemplarTypeID)
		{
			return false;
		}

		if (!stream.GetUint32(exemplarInfo.instance))
		{
			return false;
		}

		return true;
	}

	bool WriteSC4BuiltInOrdinanceProperties(cIGZOStream& stream, const BuiltInOrdinanaceExemplarInfo& exemplarInfo)
	{
		// SC4's built-in ordinances use the following property format:
		// Uint16 - Exemplar property data version (always 2)
		// Uint16 - Generic property data version (always 2).
		// Uint32 - Generic property count (always 0).
		// Uint32 - Ordinance Exemplar Group ID.
		// Uint32 - Ordinance Exemplar Type ID.
		// Uint32 - Ordinance Exemplar Instance ID.

		return stream.SetUint16(2)
			&& stream.SetUint16(2)
			&& stream.SetUint32(0)
			&& stream.SetUint32(exemplarInfo.group)
			&& stream.SetUint32(kExemplarTypeID)
			&& stream.SetUint32(exemplarInfo.instance);
	}
}


SC4BuiltInOrdinanceBase::SC4BuiltInOrdinanceBase(
	BuiltInOrdinanaceExemplarInfo info,
	const char* name,
	const StringResourceKey& nameKey,
	const char* description,
	const StringResourceKey& descriptionKey,
	uint32_t yearFirstAvailable,
	const SC4Percentage& monthlyChance, 
	int64_t enactmentIncome,
	int64_t retracmentIncome,
	int64_t monthlyConstantIncome,
	float monthlyIncomeFactor,
	uint32_t advisorID,
	bool isIncomeOrdinance,
	const OrdinancePropertyHolder& properties)
	: clsid(info.instance),
	  refCount(0),
	  name(name),
	  nameKey(nameKey),
	  description(description),
	  descriptionKey(descriptionKey),
	  yearFirstAvailable(yearFirstAvailable),
	  monthlyChance(monthlyChance),
	  enactmentIncome(enactmentIncome),
	  retracmentIncome(retracmentIncome),
	  monthlyConstantIncome(monthlyConstantIncome),
	  monthlyIncomeFactor(monthlyIncomeFactor),
	  isIncomeOrdinance(isIncomeOrdinance),
	  monthlyAdjustedIncome(0),
	  advisorID(0),
	  initialized(false),
	  available(false),
	  on(false),
	  enabled(false),
	  haveDeserialized(false),
	  pResidentialSimulator(nullptr),
	  pSimulator(nullptr),
	  miscProperties(properties),
	  exemplarInfo(info),
	  logger(Logger::GetInstance())
{
}

SC4BuiltInOrdinanceBase::SC4BuiltInOrdinanceBase(const SC4BuiltInOrdinanceBase& other)
	: clsid(other.clsid),
	  refCount(0),
	  name(other.name),
	  nameKey(other.nameKey),
	  description(other.description),
	  descriptionKey(other.descriptionKey),
	  yearFirstAvailable(other.yearFirstAvailable),
	  monthlyChance(other.monthlyChance),
	  enactmentIncome(other.enactmentIncome),
	  retracmentIncome(other.retracmentIncome),
	  monthlyConstantIncome(other.monthlyConstantIncome),
	  monthlyIncomeFactor(other.monthlyIncomeFactor),
	  isIncomeOrdinance(other.isIncomeOrdinance),
	  monthlyAdjustedIncome(other.monthlyAdjustedIncome),
	  advisorID(other.advisorID),
	  initialized(other.initialized),
	  available(other.available),
	  on(other.on),
	  enabled(other.enabled),
	  haveDeserialized(other.haveDeserialized),
	  pResidentialSimulator(other.pResidentialSimulator),
	  pSimulator(other.pSimulator),
	  miscProperties(other.miscProperties),
	  exemplarInfo(other.exemplarInfo),
	  logger(Logger::GetInstance())
{
}

SC4BuiltInOrdinanceBase::SC4BuiltInOrdinanceBase(SC4BuiltInOrdinanceBase&& other) noexcept
	: clsid(other.clsid),
	  refCount(0),
	  name(std::move(name)),
	  nameKey(other.nameKey),
	  description(std::move(other.description)),
	  descriptionKey(other.descriptionKey),
	  yearFirstAvailable(other.yearFirstAvailable),
	  monthlyChance(other.monthlyChance),
	  enactmentIncome(other.enactmentIncome),
	  retracmentIncome(other.retracmentIncome),
	  monthlyConstantIncome(other.monthlyConstantIncome),
	  monthlyIncomeFactor(other.monthlyIncomeFactor),
	  isIncomeOrdinance(other.isIncomeOrdinance),
	  monthlyAdjustedIncome(other.monthlyAdjustedIncome),
	  advisorID(other.advisorID),
	  initialized(other.initialized),
	  available(other.available),
	  on(other.on),
	  enabled(other.enabled),
	  haveDeserialized(other.haveDeserialized),
	  pResidentialSimulator(other.pResidentialSimulator),
	  pSimulator(other.pSimulator),
	  miscProperties(std::move(other.miscProperties)),
	  exemplarInfo(other.exemplarInfo),
	  logger(Logger::GetInstance())
{
	other.pResidentialSimulator = nullptr;
	other.pSimulator = nullptr;
}

SC4BuiltInOrdinanceBase& SC4BuiltInOrdinanceBase::operator=(const SC4BuiltInOrdinanceBase& other)
{
	if (this == &other)
	{
		return *this;
	}

	clsid = other.clsid;
	refCount = 0;
	name = name;
	nameKey = other.nameKey;
	description = other.description;
	descriptionKey = other.descriptionKey;
	yearFirstAvailable = other.yearFirstAvailable;
	monthlyChance = other.monthlyChance;
	enactmentIncome = other.enactmentIncome;
	retracmentIncome = other.retracmentIncome;
	monthlyConstantIncome = other.monthlyConstantIncome;
	monthlyIncomeFactor = other.monthlyIncomeFactor;
	isIncomeOrdinance = other.isIncomeOrdinance;
	monthlyAdjustedIncome = other.monthlyAdjustedIncome;
	advisorID = other.advisorID;
	initialized = other.initialized;
	available = other.available;
	on = other.on;
	enabled = other.enabled;
	haveDeserialized = other.haveDeserialized;
	exemplarInfo = other.exemplarInfo;
	pResidentialSimulator = other.pResidentialSimulator;
	pSimulator = other.pSimulator;
	miscProperties = other.miscProperties;

	return *this;
}

SC4BuiltInOrdinanceBase& SC4BuiltInOrdinanceBase::operator=(SC4BuiltInOrdinanceBase&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}

	clsid = other.clsid;
	refCount = 0;
	name = std::move(name);
	nameKey = other.nameKey;
	description = std::move(other.description);
	descriptionKey = other.descriptionKey;
	yearFirstAvailable = other.yearFirstAvailable;
	monthlyChance = other.monthlyChance;
	enactmentIncome = other.enactmentIncome;
	retracmentIncome = other.retracmentIncome;
	monthlyConstantIncome = other.monthlyConstantIncome;
	monthlyIncomeFactor = other.monthlyIncomeFactor;
	isIncomeOrdinance = other.isIncomeOrdinance;
	monthlyAdjustedIncome = other.monthlyAdjustedIncome;
	advisorID = other.advisorID;
	initialized = other.initialized;
	available = other.available;
	on = other.on;
	enabled = other.enabled;
	haveDeserialized = other.haveDeserialized;
	exemplarInfo = other.exemplarInfo;
	pResidentialSimulator = other.pResidentialSimulator;
	pSimulator = other.pSimulator;
	miscProperties = std::move(other.miscProperties);

	other.pResidentialSimulator = nullptr;
	other.pSimulator = nullptr;

	return *this;
}

bool SC4BuiltInOrdinanceBase::QueryInterface(uint32_t riid, void** ppvObj)
{
	if (riid == GZIID_SC4BuiltInOrdinanceBase)
	{
		AddRef();
		*ppvObj = this;

		return true;
	}
	else if (riid == GZIID_cISC4Ordinance)
	{
		AddRef();
		*ppvObj = static_cast<cISC4Ordinance*>(this);

		return true;
	}
	else if (riid == GZIID_cIGZSerializable)
	{
		AddRef();
		*ppvObj = static_cast<cIGZSerializable*>(this);

		return true;
	}
	else if (riid == GZIID_cIGZUnknown)
	{
		AddRef();
		*ppvObj = static_cast<cIGZUnknown*>(static_cast<cISC4Ordinance*>(this));

		return true;
	}

	return false;
}

uint32_t SC4BuiltInOrdinanceBase::AddRef()
{
	return ++refCount;
}

uint32_t SC4BuiltInOrdinanceBase::Release()
{
	if (refCount > 0)
	{
		--refCount;
	}
	return refCount;
}

bool SC4BuiltInOrdinanceBase::Init(void)
{
	if (!initialized)
	{
		enabled = true;
		initialized = true;
	}

	cISC4AppPtr pSC4App;

	if (pSC4App)
	{
		InitializeOrdinanceComponents(pSC4App->GetCity());
	}

	return true;
}

bool SC4BuiltInOrdinanceBase::Shutdown(void)
{
	enabled = false;
	initialized = false;

	cISC4AppPtr pSC4App;

	if (pSC4App)
	{
		ShutdownOrdinanceComponents(pSC4App->GetCity());
	}

	return true;
}

int64_t SC4BuiltInOrdinanceBase::GetCurrentMonthlyIncome(void)
{
	const int64_t monthlyConstantIncome = GetMonthlyConstantIncome();
	const double monthlyIncomeFactor = GetMonthlyIncomeFactor();

	if (!pResidentialSimulator)
	{
		return monthlyConstantIncome;
	}

	// The monthly income factor is multiplied by the city population.
	const int32_t cityPopulation = pResidentialSimulator->GetPopulation();
	const double populationIncome = monthlyIncomeFactor * static_cast<double>(cityPopulation);

	const double monthlyIncome = static_cast<double>(monthlyConstantIncome) + populationIncome;

	int64_t monthlyIncomeInteger = 0;

	if (monthlyIncome < std::numeric_limits<int64_t>::min())
	{
		monthlyIncomeInteger = std::numeric_limits<int64_t>::min();
	}
	else if (monthlyIncome > std::numeric_limits<int64_t>::max())
	{
		monthlyIncomeInteger = std::numeric_limits<int64_t>::max();
	}
	else
	{
		monthlyIncomeInteger = static_cast<int64_t>(monthlyIncome);
	}

	logger.WriteLineFormatted(
		LogOptions::OrdinanceAPI,
		"%s: monthly income: constant=%lld, factor=%f, population=%d, current=%lld",
		__FUNCTION__,
		monthlyConstantIncome,
		monthlyIncomeFactor,
		cityPopulation,
		monthlyIncomeInteger);

	return monthlyIncomeInteger;
}

uint32_t SC4BuiltInOrdinanceBase::GetID(void) const
{
	return clsid;
}

cIGZString* SC4BuiltInOrdinanceBase::GetName(void)
{
	return &name;
}

cIGZString* SC4BuiltInOrdinanceBase::GetDescription(void)
{
	return &description;
}

uint32_t SC4BuiltInOrdinanceBase::GetYearFirstAvailable(void)
{
	return yearFirstAvailable;
}

SC4Percentage SC4BuiltInOrdinanceBase::GetChanceAvailability(void)
{
	return monthlyChance;
}

int64_t SC4BuiltInOrdinanceBase::GetEnactmentIncome(void)
{
	logger.WriteLine(LogOptions::OrdinanceAPI, __FUNCTION__);

	return enactmentIncome;
}

int64_t SC4BuiltInOrdinanceBase::GetRetracmentIncome(void)
{
	logger.WriteLine(LogOptions::OrdinanceAPI, __FUNCTION__);

	return retracmentIncome;
}

int64_t SC4BuiltInOrdinanceBase::GetMonthlyConstantIncome(void)
{
	logger.WriteLine(LogOptions::OrdinanceAPI, __FUNCTION__);

	return monthlyConstantIncome;
}

float SC4BuiltInOrdinanceBase::GetMonthlyIncomeFactor(void)
{
	logger.WriteLine(LogOptions::OrdinanceAPI, __FUNCTION__);

	return monthlyIncomeFactor;
}

cISCPropertyHolder* SC4BuiltInOrdinanceBase::GetMiscProperties()
{
	return &miscProperties;
}

uint32_t SC4BuiltInOrdinanceBase::GetAdvisorID(void)
{
	return advisorID;
}

bool SC4BuiltInOrdinanceBase::IsAvailable(void)
{
	return available;
}

bool SC4BuiltInOrdinanceBase::IsOn(void)
{
	return available && on;
}

bool SC4BuiltInOrdinanceBase::IsEnabled(void)
{
	return enabled;
}

int64_t SC4BuiltInOrdinanceBase::GetMonthlyAdjustedIncome(void)
{
	logger.WriteLineFormatted(
		LogOptions::OrdinanceAPI,
		"%s: result=%lld",
		__FUNCTION__,
		monthlyAdjustedIncome);

	return monthlyAdjustedIncome;
}

bool SC4BuiltInOrdinanceBase::CheckConditions(void)
{
	bool result = false;

	if (enabled)
	{
		if (pSimulator)
		{
			cIGZDate* simDate = pSimulator->GetSimDate();

			if (simDate)
			{
				result = simDate->Year() >= GetYearFirstAvailable();
			}
		}
	}

	logger.WriteLineFormatted(
		LogOptions::OrdinanceAPI,
		"%s: result=%d",
		__FUNCTION__,
		result);

	return result;
}

bool SC4BuiltInOrdinanceBase::IsIncomeOrdinance(void)
{
	logger.WriteLine(LogOptions::OrdinanceAPI, __FUNCTION__);

	return isIncomeOrdinance;
}

bool SC4BuiltInOrdinanceBase::Simulate(void)
{
	monthlyAdjustedIncome = GetCurrentMonthlyIncome();

	logger.WriteLineFormatted(
		LogOptions::OrdinanceAPI,
		"%s: monthlyAdjustedIncome=%lld",
		__FUNCTION__,
		monthlyAdjustedIncome);

	return true;
}

bool SC4BuiltInOrdinanceBase::SetAvailable(bool isAvailable)
{
	logger.WriteLineFormatted(
		LogOptions::OrdinanceAPI,
		"%s: value=%d",
		__FUNCTION__,
		isAvailable);

	available = isAvailable;
	monthlyAdjustedIncome = 0;
	return true;
}

bool SC4BuiltInOrdinanceBase::SetOn(bool isOn)
{
	logger.WriteLineFormatted(
		LogOptions::OrdinanceAPI,
		"%s: value=%d",
		__FUNCTION__,
		isOn);

	on = isOn;
	return true;
}

bool SC4BuiltInOrdinanceBase::SetEnabled(bool isEnabled)
{
	logger.WriteLineFormatted(
		LogOptions::OrdinanceAPI,
		"%s: value=%d",
		__FUNCTION__,
		isEnabled);

	enabled = isEnabled;
	return true;
}

bool SC4BuiltInOrdinanceBase::ForceAvailable(bool isAvailable)
{
	return SetAvailable(isAvailable);
}

bool SC4BuiltInOrdinanceBase::ForceOn(bool isOn)
{
	return SetOn(isOn);
}

bool SC4BuiltInOrdinanceBase::ForceEnabled(bool isEnabled)
{
	return SetEnabled(isEnabled);
}

bool SC4BuiltInOrdinanceBase::ForceMonthlyAdjustedIncome(int64_t monthlyAdjustedIncome)
{
	logger.WriteLineFormatted(
		LogOptions::OrdinanceAPI,
		"%s: value=%lld",
		__FUNCTION__,
		monthlyAdjustedIncome);

	monthlyAdjustedIncome = monthlyAdjustedIncome;
	return true;
}

void SC4BuiltInOrdinanceBase::InitializeOrdinanceComponents(cISC4City* pCity)
{
	if (pCity)
	{
		if (!pResidentialSimulator)
		{
			pResidentialSimulator = pCity->GetResidentialSimulator();
		}

		if (!pSimulator)
		{
			pSimulator = pCity->GetSimulator();
		}
	}	

	LoadLocalizedStringResources();
}

void SC4BuiltInOrdinanceBase::ShutdownOrdinanceComponents(cISC4City* pCity)
{
	pResidentialSimulator = nullptr;
	pSimulator = nullptr;
}

bool SC4BuiltInOrdinanceBase::ReadBool(cIGZIStream& stream, bool& value)
{
	uint8_t temp = 0;
	// We use GetVoid because GetUint8 always returns false.
	if (!stream.GetVoid(&temp, 1))
	{
		return false;
	}

	value = temp != 0;
	return true;
}

bool SC4BuiltInOrdinanceBase::WriteBool(cIGZOStream& stream, bool value)
{
	const uint8_t uint8Value = static_cast<uint8_t>(value);

	return stream.SetVoid(&uint8Value, 1);
}

bool SC4BuiltInOrdinanceBase::Write(cIGZOStream& stream)
{
	if (stream.GetError() != 0)
	{
		return false;
	}

	const uint32_t version = 4;
	if (!stream.SetUint16(version))
	{
		return false;
	}

	if (!WriteBool(stream, initialized))
	{
		return false;
	}

	if (!stream.SetUint32(clsid))
	{
		return false;
	}

	if (!stream.SetGZStr(name))
	{
		return false;
	}

	if (!stream.SetGZStr(description))
	{
		return false;
	}

	if (!stream.SetUint32(yearFirstAvailable))
	{
		return false;
	}

	if (!stream.SetFloat32(monthlyChance.percentage))
	{
		return false;
	}

	if (!stream.SetSint64(enactmentIncome))
	{
		return false;
	}

	if (!stream.SetSint64(retracmentIncome))
	{
		return false;
	}

	if (!stream.SetSint64(monthlyConstantIncome))
	{
		return false;
	}

	if (!stream.SetFloat32(monthlyIncomeFactor))
	{
		return false;
	}

	if (!stream.SetUint32(advisorID))
	{
		return false;
	}

	if (!WriteBool(stream, available))
	{
		return false;
	}

	if (!WriteBool(stream, on))
	{
		return false;
	}

	if (!WriteBool(stream, enabled))
	{
		return false;
	}

	if (!stream.SetSint64(monthlyAdjustedIncome))
	{
		return false;
	}

	if (!WriteBool(stream, isIncomeOrdinance))
	{
		return false;
	}

	return WriteSC4BuiltInOrdinanceProperties(stream, exemplarInfo);
}

bool SC4BuiltInOrdinanceBase::Read(cIGZIStream& stream)
{
	if (stream.GetError() != 0)
	{
		return false;
	}

	uint16_t version = 0;
	if (!stream.GetUint16(version) || version != 4)
	{
		return false;
	}

	if (!ReadBool(stream, initialized))
	{
		return false;
	}

	if (!stream.GetUint32(clsid))
	{
		return false;
	}

	if (!stream.GetGZStr(name))
	{
		return false;
	}

	if (!stream.GetGZStr(description))
	{
		return false;
	}

	if (!stream.GetUint32(yearFirstAvailable))
	{
		return false;
	}

	if (!stream.GetFloat32(monthlyChance.percentage))
	{
		return false;
	}

	if (!stream.GetSint64(enactmentIncome))
	{
		return false;
	}

	if (!stream.GetSint64(retracmentIncome))
	{
		return false;
	}

	if (!stream.GetSint64(monthlyConstantIncome))
	{
		return false;
	}

	if (!stream.GetFloat32(monthlyIncomeFactor))
	{
		return false;
	}

	if (!stream.GetUint32(advisorID))
	{
		return false;
	}

	if (!ReadBool(stream, available))
	{
		return false;
	}

	if (!ReadBool(stream, on))
	{
		return false;
	}

	if (!ReadBool(stream, enabled))
	{
		return false;
	}

	if (!stream.GetSint64(monthlyAdjustedIncome))
	{
		return false;
	}

	if (!ReadBool(stream, isIncomeOrdinance))
	{
		return false;
	}

	if (!ReadSC4BuiltInOrdnanceProperties(stream, exemplarInfo))
	{
		return false;
	}

	return true;
}

uint32_t SC4BuiltInOrdinanceBase::GetGZCLSID()
{
	return clsid;
}

void SC4BuiltInOrdinanceBase::LoadLocalizedStringResources()
{
	cIGZString* localizedName = nullptr;
	cIGZString* localizedDescription = nullptr;

	if (StringResourceManager::GetLocalizedString(nameKey, &localizedName))
	{
		if (StringResourceManager::GetLocalizedString(descriptionKey, &localizedDescription))
		{
			if (localizedName->Strlen() > 0 && !localizedName->IsEqual(this->name, false))
			{
				this->name.Copy(*localizedName);
			}

			if (localizedDescription->Strlen() > 0 && !localizedDescription->IsEqual(this->description, false))
			{
				this->description.Copy(*localizedDescription);
			}

			localizedDescription->Release();
		}

		localizedName->Release();
	}
}
