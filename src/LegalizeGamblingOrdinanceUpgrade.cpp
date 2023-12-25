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

#include "LegalizeGamblingOrdinanceUpgrade.h"
#include "ISettings.h"
#include "cIGZWin.h"
#include "cISC4App.h"
#include "cISC4City.h"
#include "cISC4CivicBuildingSimulator.h"
#include "cISC4Demand.h"
#include "cISC4DemandSimulator.h"
#include "cISC4Lot.h"
#include "cISC4LotDeveloper.h"
#include "cISC4LotManager.h"
#include "cISC4Occupant.h"
#include "cISC4OccupantManager.h"
#include "cISC4View3DWin.h"
#include "cISC4ViewInputControl.h"
#include "GZServPtrs.h"

static const uint32_t kGZWin_WinSC4App = 0x6104489A;
static const uint32_t kSC4CLSID_cSC4View3DWin = 0x9A47B417;

static const uint32_t kGZIID_cISC4View3DWin = 0xFA47B3F9;

static const uint32_t kSCPROP_CityExclusionGroup = 0xEA2E078B;
static const uint32_t kOccupantGroup_Reward = 0x150B;
static const uint32_t kOccupantType_Building = 0x278128A0;

static const uint32_t kCasinoCityExclusionGroup = 0xCA78B74B;

namespace
{
	struct CasinoIteratorData
	{
		CasinoIteratorData() : casinoOccupant(nullptr)
		{
		}

		cISC4Occupant* casinoOccupant;
	};

	static bool CasinoIterator(cISC4Occupant* pOccupant, void* pData)
	{
		if (pOccupant->GetType() == kOccupantType_Building)
		{
			if (pOccupant->IsOccupantGroup(kOccupantGroup_Reward))
			{
				cISCPropertyHolder* pPoperties = pOccupant->AsPropertyHolder();

				if (pPoperties)
				{
					uint32_t cityExclusionGroup = 0;

					if (pPoperties->GetProperty(kSCPROP_CityExclusionGroup, cityExclusionGroup))
					{
						if (cityExclusionGroup == kCasinoCityExclusionGroup)
						{
							CasinoIteratorData* data = static_cast<CasinoIteratorData*>(pData);

							data->casinoOccupant = pOccupant;

							// Stop the enumeration.
							return false;
						}
					}
				}
			}
		}

		return true;
	}

	cISC4Occupant* GetCasinoOccupant(cISC4City* pCity)
	{
		cISC4OccupantManager* pOccupantManager = pCity->GetOccupantManager();

		if (pOccupantManager)
		{
			CasinoIteratorData iteratorData;

			pOccupantManager->IterateOccupants(CasinoIterator, &iteratorData, nullptr, nullptr, kOccupantType_Building);

			return iteratorData.casinoOccupant;
		}

		return nullptr;
	}

	OrdinancePropertyHolder CreateDefaultOrdinanceEffects()
	{
		OrdinancePropertyHolder properties;

		// Crime Effect: +20%
		properties.AddProperty(0x28ed0380, 1.20f);

		return properties;
	}

	void DemolishCasino(cISC4City* pCity)
	{
		cISC4Occupant* pCasinoOccupant = GetCasinoOccupant(pCity);

		if (pCasinoOccupant)
		{
			cISC4LotManager* pLotManager = pCity->GetLotManager();

			if (pLotManager)
			{
				cISC4Lot* pCasinoLot = pLotManager->GetOccupantLot(pCasinoOccupant);

				if (pCasinoLot)
				{
					cISC4LotDeveloper* pLotDeveloper = pCity->GetLotDeveloper();

					if (pLotDeveloper)
					{
						pLotDeveloper->StartDemolishLot(pCasinoLot);
						pLotDeveloper->EndDemolishLot(pCasinoLot);
					}
				}
			}
		}
	}

	void DisableCasinoMenuItem(cISC4App* pSC4App, cISC4City* pCity)
	{
		constexpr uint32_t casinoBuildingID = 0x33a0000;

		// Disable the Casino item in the Rewards menu.

		cISC4CivicBuildingSimulator* pCivicBuildingSim = pCity->GetCivicBuildingSimulator();

		if (pCivicBuildingSim)
		{
			const cISC4CivicBuildingSimulator::ConditionalBuildingStatus* status = pCivicBuildingSim->GetConditionalBuildingStatus(casinoBuildingID);

			if (status)
			{
				constexpr int16_t Status_BuildingDisabled = 1;

				// We create a copy of the existing ConditionalBuildingStatus class and
				// disable the Casino building in the Rewards menu.

				cISC4CivicBuildingSimulator::ConditionalBuildingStatus newStatus(*status);
				newStatus.status = Status_BuildingDisabled;

				pCivicBuildingSim->UpdateConditionalBuildingStatus(casinoBuildingID, &newStatus);
			}
		}

		// Turn off the Place Lot control, if it is active.

		cIGZWin* pMainWin = pSC4App->GetMainWindow();

		if (pMainWin)
		{
			cIGZWin* pParentWin = pMainWin->GetChildWindowFromID(kGZWin_WinSC4App);

			if (pParentWin)
			{
				cISC4View3DWin* pView3DWin = nullptr;

				if (pParentWin->GetChildAs(kSC4CLSID_cSC4View3DWin, kGZIID_cISC4View3DWin, reinterpret_cast<void**>(&pView3DWin)))
				{
					cISC4ViewInputControl* pCurrentViewInputControl = pView3DWin->GetCurrentViewInputControl();

					if (pCurrentViewInputControl)
					{
						constexpr uint32_t placeLotViewInputControl = 0x88F154FBl;

						if (pCurrentViewInputControl->GetID() == placeLotViewInputControl)
						{
							pView3DWin->RemoveCurrentViewInputControl(false);
						}
					}
				}
			}
		}
	}
}

LegalizeGamblingOrdinanceUpgrade::LegalizeGamblingOrdinanceUpgrade()
	: SC4BuiltInOrdinanceBase(
		BuiltInOrdinanaceExemplarInfo(0xA9C2C209, 0xA0D07129),
		"Legalize Gambling",
		StringResourceKey(0x6A231EAA, 0x2A5EA6BF),
		"Opens the doors for casino operators to set up business.  Deals can be cut with casino operators"
		" for income but these come at the cost of local Mayor Rating and potential crime elements.",
		StringResourceKey(0x6A231EAA, 0x0A5EA6BF),
		/* year first available */ 0,
		/* monthly chance */ SC4Percentage(0.005f),
		/* enactment income */		  0,
		/* retracment income */       -20,
		/* monthly constant income */ 100, // Only for the save game, we use the baseMonthlyIncome value in our calculations.
		/* monthly income factor */   1.0f, // Only for the save game, we use the wealth factor values in our calculations.
		/* advisor ID */ 0,
		/* income ordinance */		  true,
		CreateDefaultOrdinanceEffects()),
		pDemandSimulator(nullptr),
	    baseMonthlyIncome(100),
		residentialLowWealthIncomeFactor(0.05f),
		residentialMedWealthIncomeFactor(0.03f),
		residentialHighWealthIncomeFactor(0.01f),
	    ignoreSetOnCallCount(0)
{
}

int64_t LegalizeGamblingOrdinanceUpgrade::GetCurrentMonthlyIncome()
{
	// We use our own monthly income value instead of the one in the base class.
	// This prevents our values from altering the save game data, and vice versa.

	double monthlyIncome = static_cast<double>(baseMonthlyIncome);

	// Add the monthly income for each of the residential wealth groups.
	// If the income factor is 0.0 for any group they will not participate
	// in the Legalize Gambling ordinance income.

	if (residentialLowWealthIncomeFactor > 0.0f)
	{
		const double lowWealthPopulation = GetCityPopulation(0x1011);
		if (lowWealthPopulation > 0.0)
		{
			const double gamblingPopulationIncome = lowWealthPopulation * static_cast<double>(residentialLowWealthIncomeFactor);

			monthlyIncome += gamblingPopulationIncome;
		}
	}

	if (residentialMedWealthIncomeFactor > 0.0f)
	{
		const double medWealthPopulation = GetCityPopulation(0x1021);
		if (medWealthPopulation > 0.0)
		{
			const double gamblingPopulationIncome = medWealthPopulation * static_cast<double>(residentialMedWealthIncomeFactor);

			monthlyIncome += gamblingPopulationIncome;
		}
	}

	if (residentialHighWealthIncomeFactor > 0.0f)
	{
		const double highWealthPopulation = GetCityPopulation(0x1031);
		if (highWealthPopulation > 0.0)
		{
			const double gamblingPopulationIncome = highWealthPopulation * static_cast<double>(residentialHighWealthIncomeFactor);

			monthlyIncome += gamblingPopulationIncome;
		}
	}

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
		"%s: monthly income: base=%lld, R$ factor=%f, R$$ factor=%f, R$$$ factor=%f, current=%lld",
		__FUNCTION__,
		baseMonthlyIncome,
		residentialLowWealthIncomeFactor,
		residentialMedWealthIncomeFactor,
		residentialHighWealthIncomeFactor,
		monthlyIncomeInteger);

	return monthlyIncomeInteger;
}

bool LegalizeGamblingOrdinanceUpgrade::SetOn(bool isOn)
{
	// The ordinance simulator turns the ordinance off and on when adding or removing it.
	// Because this ordinance destroys the Casino building when it is turned off, we ignore
	// the calls that the ordinance simulator sends when adding or removing the ordinance.

	if (ignoreSetOnCallCount == 0)
	{
		SC4BuiltInOrdinanceBase::SetOn(isOn);

		if (!isOn)
		{
			cISC4AppPtr pSC4App;

			if (pSC4App)
			{
				cISC4City* pCity = pSC4App->GetCity();

				if (pCity)
				{
					DemolishCasino(pCity);
					DisableCasinoMenuItem(pSC4App, pCity);
				}
			}
		}
	}

    return true;
}

void LegalizeGamblingOrdinanceUpgrade::InitializeOrdinanceComponents(cISC4City* pCity)
{
	SC4BuiltInOrdinanceBase::InitializeOrdinanceComponents(pCity);

	if (pCity)
	{
		pDemandSimulator = pCity->GetDemandSimulator();
	}
}

void LegalizeGamblingOrdinanceUpgrade::ShutdownOrdinanceComponents(cISC4City* pCity)
{
	SC4BuiltInOrdinanceBase::ShutdownOrdinanceComponents(pCity);
	pDemandSimulator = nullptr;
}

void LegalizeGamblingOrdinanceUpgrade::PushIgnoreSetOnCalls()
{
	++ignoreSetOnCallCount;
}

void LegalizeGamblingOrdinanceUpgrade::PopIgnoreSetOnCalls()
{
	if (ignoreSetOnCallCount > 0)
	{
		--ignoreSetOnCallCount;
	}
}

void LegalizeGamblingOrdinanceUpgrade::UpdateOrdinanceData(const ISettings& settings)
{
	this->baseMonthlyIncome = settings.BaseMonthlyIncome();
	this->residentialLowWealthIncomeFactor = settings.ResidentialLowWealthFactor();
	this->residentialMedWealthIncomeFactor = settings.ResidentialMedWealthFactor();
	this->residentialHighWealthIncomeFactor = settings.ResidentialHighWealthFactor();
	this->miscProperties = settings.OrdinanceEffects();
}

float LegalizeGamblingOrdinanceUpgrade::GetCityPopulation(uint32_t groupID)
{
	float value = 0.0f;

	if (pDemandSimulator)
	{
		constexpr uint32_t cityCensusIndex = 0;

		const cISC4Demand* demand = pDemandSimulator->GetDemand(groupID, cityCensusIndex);

		if (demand)
		{
			value = demand->QuerySupplyValue();
		}
	}

	return value;
}
