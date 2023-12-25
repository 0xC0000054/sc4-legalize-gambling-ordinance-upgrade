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

#include "version.h"
#include "LegalizeGamblingOrdinanceUpgrade.h"
#include "Logger.h"
#include "OrdinancePropertyHolder.h"
#include "Settings.h"
#include "cIGZFrameWork.h"
#include "cIGZApp.h"
#include "cISC4App.h"
#include "cISC4City.h"
#include "cISC4CivicBuildingSimulator.h"
#include "cISC4Ordinance.h"
#include "cISC4OrdinanceSimulator.h"
#include "cISC4ResidentialSimulator.h"
#include "cISC4Simulator.h"
#include "cIGZMessageServer2.h"
#include "cIGZMessageTarget.h"
#include "cIGZMessageTarget2.h"
#include "cIGZString.h"
#include "cRZMessage2COMDirector.h"
#include "cRZMessage2Standard.h"
#include "cRZBaseString.h"
#include "GZServPtrs.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <Windows.h>
#include "wil/resource.h"
#include "wil/win32_helpers.h"

static constexpr uint32_t kSC4MessagePostCityInit = 0x26d31EC1;
static constexpr uint32_t kSC4MessagePreCityShutdown = 0x26D31EC2;

static constexpr uint32_t kLegalizeGamblingUpgradePluginDirectorID = 0x464631d7;

static constexpr std::string_view PluginConfigFileName = "SC4LegalizeGamblingUpgrade.ini";
static constexpr std::string_view PluginLogFileName = "SC4LegalizeGamblingUpgrade.log";

class LegalizeGamblingUpgradeDllDirector : public cRZMessage2COMDirector
{
public:

	LegalizeGamblingUpgradeDllDirector()
	{
		std::filesystem::path dllFolderPath = GetDllFolderPath();

		configFilePath = dllFolderPath;
		configFilePath /= PluginConfigFileName;

		std::filesystem::path logFilePath = dllFolderPath;
		logFilePath /= PluginLogFileName;

		Logger& logger = Logger::GetInstance();
		logger.Init(logFilePath, LogOptions::Errors);
		logger.WriteLogFileHeader("SC4LegalizeGamblingUpgrade v" PLUGIN_VERSION_STR);
	}

	uint32_t GetDirectorID() const
	{
		return kLegalizeGamblingUpgradePluginDirectorID;
	}

	void EnumClassObjects(cIGZCOMDirector::ClassObjectEnumerationCallback pCallback, void* pContext)
	{
		// The classes you want to add must be initialized in the DLL constructor because
		// the framework calls this method before OnStart or any of the hook callbacks.
		// This method is called once when initializing a director, the list of class IDs
		// it returns is cached by the framework.
		//
		// The second parameter of the ClassObjectEnumerationCallback is a class version number.
		// If the ID matches a class that has already been registered, the framework will replace
		// the existing class if the new class has a higher version number.
		//
		// SC4's built-in ordinances are registered with a version of 0, so using 1 will allow
		// our class to replace SC4's built-in version.
		pCallback(legalizeGamblingOrdinanceUpgrade.GetID(), 1, pContext);
	}

	bool GetClassObject(uint32_t rclsid, uint32_t riid, void** ppvObj)
	{
		// To retrieve an instance of a registered class the framework will call the
		// GetClassObject method whenever it needs the director to provide one.

		bool result = false;

		if (rclsid == legalizeGamblingOrdinanceUpgrade.GetID())
		{
			result = legalizeGamblingOrdinanceUpgrade.QueryInterface(riid, ppvObj);
		}

		return result;
	}

	void DumpConditionalBuildingStatus(cISC4City* pCity)
	{
		if (pCity)
		{
			constexpr uint32_t casinoBuildingID = 0x33a0000;

			cISC4CivicBuildingSimulator* pCivicBuildingSim = pCity->GetCivicBuildingSimulator();

			if (pCivicBuildingSim)
			{
				const auto conditionalBuildingStatus = pCivicBuildingSim->GetConditionalBuildingStatus(casinoBuildingID);

				if (conditionalBuildingStatus)
				{
					Logger& logger = Logger::GetInstance();

					logger.WriteLineFormatted(
						LogOptions::Errors,
						"status = %d, building_count=%d, unknown1==0x%p, unknown2=0x%p, unknown3=0x%p, unknown4=0x%p, unknown5=0x%p",
						conditionalBuildingStatus->status,
						conditionalBuildingStatus->buildingCount,
						conditionalBuildingStatus->unknown1,
						conditionalBuildingStatus->unknown2,
						conditionalBuildingStatus->unknown3,
						conditionalBuildingStatus->unknown4,
						conditionalBuildingStatus->unknown5);
				}
			}
		}
	}

	void PostCityInit(cIGZMessage2Standard* pStandardMsg)
	{
		cISC4City* pCity = reinterpret_cast<cISC4City*>(pStandardMsg->GetIGZUnknown());

		if (pCity)
		{
			//DumpConditionalBuildingStatus(pCity);

			cISC4OrdinanceSimulator* pOrdinanceSimulator = pCity->GetOrdinanceSimulator();

			if (pOrdinanceSimulator)
			{
				// Only add the ordinance if it is not already present. If it is part
				// of the city save file it may have already been loaded at this point.

				cISC4Ordinance* pOrdinance = pOrdinanceSimulator->GetOrdinanceByID(legalizeGamblingOrdinanceUpgrade.GetID());

				if (pOrdinance)
				{
					LegalizeGamblingOrdinanceUpgrade* item = reinterpret_cast<LegalizeGamblingOrdinanceUpgrade*>(pOrdinance);

					item->Init();
					item->UpdateOrdinanceData(settings);
				}
				else
				{
					legalizeGamblingOrdinanceUpgrade.Init();
					legalizeGamblingOrdinanceUpgrade.UpdateOrdinanceData(settings);

					// The ordinance simulator turns the ordinance off and on when adding or removing it.
					// Because this ordinance destroys the Casino building when it is turned off, we ignore
					// the calls that the ordinance simulator sends when adding or removing the ordinance.
					legalizeGamblingOrdinanceUpgrade.PushIgnoreSetOnCalls();

					pOrdinanceSimulator->AddOrdinance(legalizeGamblingOrdinanceUpgrade);

					legalizeGamblingOrdinanceUpgrade.PopIgnoreSetOnCalls();
				}

				//DumpRegisteredOrdinances(pCity, pOrdinanceSimulator);
			}
		}
	}

	void PreCityShutdown(cIGZMessage2Standard* pStandardMsg)
	{
		cISC4City* pCity = reinterpret_cast<cISC4City*>(pStandardMsg->GetIGZUnknown());

		if (pCity)
		{
			//DumpConditionalBuildingStatus(pCity);

			cISC4OrdinanceSimulator* pOrdinanceSimulator = pCity->GetOrdinanceSimulator();

			if (pOrdinanceSimulator)
			{
				legalizeGamblingOrdinanceUpgrade.Shutdown();

				cISC4Ordinance* pOrdinance = pOrdinanceSimulator->GetOrdinanceByID(legalizeGamblingOrdinanceUpgrade.GetID());

				if (pOrdinance)
				{
					pOrdinance->Shutdown();
				}

				// The ordinance simulator turns the ordinance off and on when adding or removing it.
				// Because this ordinance destroys the Casino building when it is turned off, we ignore
				// the calls that the ordinance simulator sends when adding or removing the ordinance.
				legalizeGamblingOrdinanceUpgrade.PushIgnoreSetOnCalls();

				pOrdinanceSimulator->RemoveOrdinance(legalizeGamblingOrdinanceUpgrade);

				legalizeGamblingOrdinanceUpgrade.PopIgnoreSetOnCalls();
			}
		}
	}

	bool DoMessage(cIGZMessage2* pMessage)
	{
		cIGZMessage2Standard* pStandardMsg = static_cast<cIGZMessage2Standard*>(pMessage);
		uint32_t dwType = pMessage->GetType();

		switch (dwType)
		{
		case kSC4MessagePostCityInit:
			PostCityInit(pStandardMsg);
			break;
		case kSC4MessagePreCityShutdown:
			PreCityShutdown(pStandardMsg);
			break;
		}

		return true;
	}

	bool PostAppInit()
	{
		Logger& logger = Logger::GetInstance();

		try
		{
			settings.Load(configFilePath);
		}
		catch (const std::exception& e)
		{
			logger.WriteLine(LogOptions::Errors, e.what());
			return false;
		}

		cIGZMessageServer2Ptr pMsgServ;
		if (pMsgServ)
		{
			std::vector<uint32_t> requiredNotifications;
			requiredNotifications.push_back(kSC4MessagePostCityInit);
			requiredNotifications.push_back(kSC4MessagePreCityShutdown);

			for (uint32_t messageID : requiredNotifications)
			{
				if (!pMsgServ->AddNotification(this, messageID))
				{
					logger.WriteLine(LogOptions::Errors, "Failed to subscribe to the required notifications.");
					return false;
				}
			}
		}
		else
		{
			logger.WriteLine(LogOptions::Errors, "Failed to subscribe to the required notifications.");
			return false;
		}

		logger.WriteLine(LogOptions::Info, "Plugin loaded.");
		return true;
	}

	bool OnStart(cIGZCOM* pCOM)
	{
		cIGZFrameWork* const pFramework = RZGetFrameWork();

		if (pFramework->GetState() < cIGZFrameWork::kStatePreAppInit)
		{
			pFramework->AddHook(this);
		}
		else
		{
			PreAppInit();
		}
		return true;
	}

private:

	std::filesystem::path GetDllFolderPath()
	{
		wil::unique_cotaskmem_string modulePath = wil::GetModuleFileNameW(wil::GetModuleInstanceHandle());

		std::filesystem::path temp(modulePath.get());

		return temp.parent_path();
	}

	void DumpRegisteredOrdinances(cISC4City* pCity, cISC4OrdinanceSimulator* pOrdinanceSimulator)
	{
		Logger& logger = Logger::GetInstance();

		if (!logger.IsEnabled(LogOptions::DumpRegisteredOrdinances))
		{
			return;
		}

		uint32_t dwCountOut = 0;

		uint32_t registeredOrdinances = pOrdinanceSimulator->GetOrdinanceIDArray(nullptr, dwCountOut);

		logger.WriteLineFormatted(
			LogOptions::DumpRegisteredOrdinances,
			"The game has %d ordinances registered.",
			registeredOrdinances);

		if (registeredOrdinances > 0)
		{
			std::vector<uint32_t> registeredOrdinanceIDs(static_cast<size_t>(registeredOrdinances));
			uint32_t ordinancesRequested = registeredOrdinances;

			uint32_t ordinancesFetched = pOrdinanceSimulator->GetOrdinanceIDArray(registeredOrdinanceIDs.data(), ordinancesRequested);

			if (ordinancesFetched > 0)
			{
				int32_t cityPopulation = -1;
				cISC4ResidentialSimulator* pResidentialSimulator = pCity->GetResidentialSimulator();
				if (pResidentialSimulator)
				{
					cityPopulation = pResidentialSimulator->GetPopulation();
				}

				for (uint32_t i = 0; i < ordinancesFetched; i++)
				{
					uint32_t clsid = registeredOrdinanceIDs[i];

					cISC4Ordinance* pOrdinance = pOrdinanceSimulator->GetOrdinanceByID(clsid);

					if (pOrdinance)
					{
						uint32_t id = pOrdinance->GetID();
						cIGZString* name = pOrdinance->GetName();
						bool isIncome = pOrdinance->IsIncomeOrdinance();
						int64_t enactmentIncome = pOrdinance->GetEnactmentIncome();
						int64_t retracmentIncome = pOrdinance->GetRetracmentIncome();
						int64_t monthlyConstantIncome = pOrdinance->GetMonthlyConstantIncome();
						float monthlyIncomeFactor = pOrdinance->GetMonthlyIncomeFactor();
						int64_t currentMonthlyIncome = pOrdinance->GetCurrentMonthlyIncome();

						if (name)
						{
							logger.WriteLineFormatted(
								LogOptions::DumpRegisteredOrdinances,
								"0x%08x = %s, income=%s, enactment=%lld, retracment=%lld, monthly: constant=%lld, factor=%f, current=%lld, city population=%d",
								clsid,
								name->ToChar(),
								isIncome ? "true" : "false",
								enactmentIncome,
								retracmentIncome,
								monthlyConstantIncome,
								monthlyIncomeFactor,
								currentMonthlyIncome,
								cityPopulation);
						}
						else
						{
							logger.WriteLineFormatted(LogOptions::DumpRegisteredOrdinances, "0x%08x", clsid);
						}
					}
					else
					{
						logger.WriteLineFormatted(LogOptions::DumpRegisteredOrdinances, "0x%08x", clsid);
					}
				}
			}
		}
	}

	std::filesystem::path configFilePath;
	Settings settings;
	LegalizeGamblingOrdinanceUpgrade legalizeGamblingOrdinanceUpgrade;
};

cRZCOMDllDirector* RZGetCOMDllDirector() {
	static LegalizeGamblingUpgradeDllDirector sDirector;
	return &sDirector;
}