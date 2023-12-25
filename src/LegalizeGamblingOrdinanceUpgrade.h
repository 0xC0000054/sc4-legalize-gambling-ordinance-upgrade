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

#pragma once
#include "SC4BuiltInOrdinanceBase.h"

class cISC4City;
class cISC4DemandSimulator;
class cISC4Occupant;
class cISC4OccupantManager;
class ISettings;

class LegalizeGamblingOrdinanceUpgrade final : public SC4BuiltInOrdinanceBase
{
public:

	LegalizeGamblingOrdinanceUpgrade();

	int64_t GetCurrentMonthlyIncome() override;

	bool SetOn(bool isOn) override;

	void PushIgnoreSetOnCalls();
	void PopIgnoreSetOnCalls();

	void UpdateOrdinanceData(const ISettings& settings);

private:

	float GetCityPopulation(uint32_t groupID);
	void InitializeOrdinanceComponents(cISC4City* pCity) override;
	void ShutdownOrdinanceComponents(cISC4City* pCity) override;

	cISC4DemandSimulator* pDemandSimulator;

	// We use our own fields for the current monthly income calculations.
	// This is done to avoid modifying that data in the save game.

	int64_t baseMonthlyIncome;
	float residentialLowWealthIncomeFactor;
	float residentialMedWealthIncomeFactor;
	float residentialHighWealthIncomeFactor;

	
	uint32_t ignoreSetOnCallCount;
};
